//
//  ViewController.m
//  FishhookDemo
//
//  Created by DengBin on 2019/3/21.
//  Copyright © 2019 DengBin. All rights reserved.
//

#import "ViewController.h"

@interface ViewController ()<UITabBarDelegate,UITableViewDataSource>

@property (weak, nonatomic) IBOutlet UITableView *tableView;

/* titles */
@property (nonatomic,strong) NSArray *titles;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.titles = @[@"runtime",@"fishhook"];
    // Do any additional setup after loading the view, typically from a nib.
}

#pragma mark - UITableViewDataSource && UITableViewDelegate
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{
    return self.titles.count;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath{
    return 44;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath{
    static NSString *cellIdentifier = @"cellIdentifier";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];
    if ( !cell ) {
        cell = [[UITableViewCell alloc]initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cellIdentifier];
        cell.accessoryType = UITableViewCellAccessoryNone;
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
        cell.backgroundColor = [UIColor whiteColor];
        cell.contentView.backgroundColor = [UIColor whiteColor];
    }
    cell.textLabel.text = self.titles[indexPath.row];
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
}
@end
