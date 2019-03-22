//
//  FishhookVC.m
//  FishhookDemo
//
//  Created by DengBin on 2019/3/21.
//  Copyright © 2019 DengBin. All rights reserved.
//

#import "FishhookVC.h"
#import "fishhook.h"

@interface FishhookVC ()

@end

@implementation FishhookVC

- (void)viewDidLoad {
    [super viewDidLoad];
    NSLog(@"log来了，老弟");
    
    struct rebinding nslog;
    nslog.name = "NSLog";
    nslog.replacement = my_nslog;
    nslog.replaced = (void *)&sys_nslog;
    struct rebinding rebs[1] = {nslog};
    rebind_symbols(rebs, 1);
    
}
//---------------------------------更改NSLog-----------
//函数指针
static void(*sys_nslog)(NSString * format,...);

//定义一个新的函数
void my_nslog(NSString * format,...){
    format = [format stringByAppendingString:@"你咋又来了 \n"];
    //调用原始的
    sys_nslog(format);
}



-(void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    NSLog(@"log又来了，老弟！！");
}

@end
