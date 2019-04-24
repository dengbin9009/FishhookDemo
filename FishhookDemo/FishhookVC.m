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
    add(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,14,15,16,17,18,19,20,21,22,23,2425,26,27,28,29,30,31,32,33);
//    add3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,14,15,16,17,18,19,20,21);
}

int add(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l,int m,int m1,int m2,int m3,int m4,int m5,int m6,int m7,int m8,int n1,int n2,int n3,int n4,int n5,int n6,int n7,int n8,int n9,int n10,int n11) {
    int xx = a+b+c+d+e+f+g+h+i+j+k+l+m+m1+m2+m3+m4+m5+m6+m7+m8;
    return xx;
}

int add2(int a,int b,int c,int d,int e) {
    int xx = a+b+c+d+e;
    return xx;
}

int add3(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l,int m,int m1,int m2,int m3,int m4,int m5,int m6,int m7,int m8) {
    int xx = a+b+c+d+e+f+g+h+i+j+k+l+m+m1+m2+m3+m4+m5+m6+m7+m8;
    return xx;
}

@end
