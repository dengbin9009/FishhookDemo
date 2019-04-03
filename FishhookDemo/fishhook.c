// Copyright (c) 2013, Facebook, Inc.
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name Facebook nor the names of its contributors may be used to
//     endorse or promote products derived from this software without specific
//     prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "fishhook.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#ifdef __LP64__
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT
#endif

#ifndef SEG_DATA_CONST
#define SEG_DATA_CONST  "__DATA_CONST"
#endif

struct rebindings_entry {
    struct rebinding *rebindings;
    size_t rebindings_nel;
    struct rebindings_entry *next;
};

static struct rebindings_entry *_rebindings_head;

// 给需要rebinding的方法结构体开辟出对应的空间
// 生成对应的链表结构（rebindings_entry）
static int prepend_rebindings(struct rebindings_entry **rebindings_head,
                              struct rebinding rebindings[],
                              size_t nel) {
    // 开辟一个rebindings_entry大小的空间
    struct rebindings_entry *new_entry = (struct rebindings_entry *) malloc(sizeof(struct rebindings_entry));
    if (!new_entry) {
        return -1;
    }
    // 一共有nel个rebinding
    new_entry->rebindings = (struct rebinding *) malloc(sizeof(struct rebinding) * nel);
    if (!new_entry->rebindings) {
        free(new_entry);
        return -1;
    }
    // 将rebinding赋值给new_entry->rebindings
    memcpy(new_entry->rebindings, rebindings, sizeof(struct rebinding) * nel);
    // 继续赋值nel
    new_entry->rebindings_nel = nel;
    // 每次都将new_entry插入头部
    new_entry->next = *rebindings_head;
    // rebindings_head重新指向头部
    *rebindings_head = new_entry;
    return 0;
}

static void perform_rebinding_with_section(struct rebindings_entry *rebindings,
                                           section_t *section,
                                           intptr_t slide,
                                           nlist_t *symtab,
                                           char *strtab,
                                           uint32_t *indirect_symtab) {
    // reserved1对应的的是indirect_symbol中的offset，也就是indirect_symbol的真实地址
    // indirect_symtab+offset就是indirect_symbol_indices(indirect_symbol的数组)
    uint32_t *indirect_symbol_indices = indirect_symtab + section->reserved1;
    // 函数地址，addr就是section的偏移地址
    void **indirect_symbol_bindings = (void **)((uintptr_t)slide + section->addr);
    // 遍历section中的每个符号
    for (uint i = 0; i < section->size / sizeof(void *); i++) {
        // 访问indirect_symbol，symtab_index就是indirect_symbol中data的值
        uint32_t symtab_index = indirect_symbol_indices[i];
        if (symtab_index == INDIRECT_SYMBOL_ABS || symtab_index == INDIRECT_SYMBOL_LOCAL ||
            symtab_index == (INDIRECT_SYMBOL_LOCAL   | INDIRECT_SYMBOL_ABS)) {
            continue;
        }
        // 访问symbol_table，根据symtab_index获取到symbol_table中的偏移offset
        uint32_t strtab_offset = symtab[symtab_index].n_un.n_strx;
        // 访问string_table，根据strtab_offset获取symbol_name
        char *symbol_name = strtab + strtab_offset;
        // string_table中的所有函数名都是以"."开始的，所以一个函数一定有两个字符
        bool symbol_name_longer_than_1 = symbol_name[0] && symbol_name[1];
        struct rebindings_entry *cur = rebindings;
        // 已经存入的rebindings_entry
        while (cur) {
            // 循环每个entry中需要重绑定的函数
            for (uint j = 0; j < cur->rebindings_nel; j++) {
                // 判断symbol_name是否是一个正确的函数名
                // 需要被重绑定的函数名是否与当前symbol_name相等
                if (symbol_name_longer_than_1 &&
                    strcmp(&symbol_name[1], cur->rebindings[j].name) == 0) {
                    // 判断replaced是否存在
                    // 判断replaced和老的函数是否是一样的
                    if (cur->rebindings[j].replaced != NULL &&
                        indirect_symbol_bindings[i] != cur->rebindings[j].replacement) {
                        // 将原函数的地址给新函数replaced
                        *(cur->rebindings[j].replaced) = indirect_symbol_bindings[i];
                    }
                    // 将replacement赋值给刚刚找到的
                    indirect_symbol_bindings[i] = cur->rebindings[j].replacement;
                    goto symbol_loop;
                }
            }
            // 继续下一个需要绑定的函数
            cur = cur->next;
        }
    symbol_loop:;
    }
}

static void rebind_symbols_for_image(struct rebindings_entry *rebindings,
                                     const struct mach_header *header,
                                     intptr_t slide) {
    Dl_info info;
    // 判断当前macho是否在进程里，如果不在则直接返回
    if (dladdr(header, &info) == 0) {
        return;
    }
    
    // 定义好几个变量，后面去遍历查找
    segment_command_t *cur_seg_cmd;
    // MachO中Load Commons中的linkedit
    segment_command_t *linkedit_segment = NULL;
    // MachO中LC_SYMTAB
    struct symtab_command* symtab_cmd = NULL;
    // MachO中LC_DYSYMTAB
    struct dysymtab_command* dysymtab_cmd = NULL;
    
    // header的首地址+mach_header的内存大小
    // 得到跳过mach_header的地址,也就是直接到Load Commons的地址
    uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);
    // 遍历Load Commons 找到上面三个遍历
    for (uint i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize) {
        cur_seg_cmd = (segment_command_t *)cur;
        // 如果是LC_SEGMENT_64
        if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            // 找到linkedit
            if (strcmp(cur_seg_cmd->segname, SEG_LINKEDIT) == 0) {
                linkedit_segment = cur_seg_cmd;
            }
        }
        // 如果是LC_SYMTAB,就找到了symtab_cmd
        else if (cur_seg_cmd->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command*)cur_seg_cmd;
        }
        // 如果是LC_DYSYMTAB,就找到了dysymtab_cmd
        else if (cur_seg_cmd->cmd == LC_DYSYMTAB) {
            dysymtab_cmd = (struct dysymtab_command*)cur_seg_cmd;
        }
    }
    // 下面其中任何一个值没有都直接return
    // 因为image不是需要找的image
    if (!symtab_cmd || !dysymtab_cmd || !linkedit_segment ||
        !dysymtab_cmd->nindirectsyms) {
        return;
    }
    
    // Find base symbol/string table addresses
    // 找到linkedit的头地址
    // linkedit_base其实就是MachO的头地址！！！可以通过查看linkedit_base值和image list命令查看验证！！！
    /**********************************************************
     Linkedit虚拟地址 = PAGEZERO(64位下1G) + FileOffset
     MachO地址 = PAGEZERO + ASLR
     上面两个公式是已知的 得到下面这个公式
     MachO文件地址 = Linkedit虚拟地址 - FileOffset + ASLR(slide)
     **********************************************************/
    uintptr_t linkedit_base = (uintptr_t)slide + linkedit_segment->vmaddr - linkedit_segment->fileoff;
    // 获取symbol_table的真实地址
    nlist_t *symtab = (nlist_t *)(linkedit_base + symtab_cmd->symoff);
    // 获取string_table的真实地址
    char *strtab = (char *)(linkedit_base + symtab_cmd->stroff);
    
    // Get indirect symbol table (array of uint32_t indices into symbol table)
    // 获取indirect_symtab的真实地址
    uint32_t *indirect_symtab = (uint32_t *)(linkedit_base + dysymtab_cmd->indirectsymoff);
    // 同样的，得到跳过mach_header的地址,得到Load Commons的地址
    cur = (uintptr_t)header + sizeof(mach_header_t);
    // 遍历Load Commons，找到对应符号进行重新绑定
    for (uint i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize) {
        cur_seg_cmd = (segment_command_t *)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            // 如果不是__DATA段，也不是__DATA_CONST段，直接跳过
            if (strcmp(cur_seg_cmd->segname, SEG_DATA) != 0 &&
                strcmp(cur_seg_cmd->segname, SEG_DATA_CONST) != 0) {
                continue;
            }
            // 遍历所有的segment
            for (uint j = 0; j < cur_seg_cmd->nsects; j++) {
                section_t *sect =
                (section_t *)(cur + sizeof(segment_command_t)) + j;
                // 找懒加载表S_LAZY_SYMBOL_POINTERS
                if ((sect->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS) {
                    // 重绑定的真正函数
                    perform_rebinding_with_section(rebindings, sect, slide, symtab, strtab, indirect_symtab);
                }
                // 找非懒加载表S_NON_LAZY_SYMBOL_POINTERS
                if ((sect->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
                    // 重绑定的真正函数
                    perform_rebinding_with_section(rebindings, sect, slide, symtab, strtab, indirect_symtab);
                }
            }
        }
    }
}

static void _rebind_symbols_for_image(const struct mach_header *header,
                                      intptr_t slide) {
    // 找到对应的符号，进行重绑定
    rebind_symbols_for_image(_rebindings_head, header, slide);
}

// 在知道确定的MachO，可以使用该方法
int rebind_symbols_image(void *header,
                         intptr_t slide,
                         struct rebinding rebindings[],
                         size_t rebindings_nel) {
    struct rebindings_entry *rebindings_head = NULL;
    int retval = prepend_rebindings(&rebindings_head, rebindings, rebindings_nel);
    rebind_symbols_for_image(rebindings_head, (const struct mach_header *) header, slide);
    if (rebindings_head) {
        free(rebindings_head->rebindings);
    }
    free(rebindings_head);
    return retval;
}

int rebind_symbols(struct rebinding rebindings[], size_t rebindings_nel) {
    int retval = prepend_rebindings(&_rebindings_head, rebindings, rebindings_nel);
    if (retval < 0) {
        return retval;
    }
    // If this was the first call, register callback for image additions (which is also invoked for
    // existing images, otherwise, just run on existing images
    if (!_rebindings_head->next) {
        // 向每个image注册_rebind_symbols_for_image函数，并且立即触发一次
        _dyld_register_func_for_add_image(_rebind_symbols_for_image);
    } else {
        // _dyld_image_count() 获取image数量
        uint32_t c = _dyld_image_count();
        for (uint32_t i = 0; i < c; i++) {
            // _dyld_get_image_header(i) 获取第i个image的header指针
            // _dyld_get_image_vmaddr_slide(i) 获取第i个image的基址
            _rebind_symbols_for_image(_dyld_get_image_header(i), _dyld_get_image_vmaddr_slide(i));
        }
    }
    return retval;
}

