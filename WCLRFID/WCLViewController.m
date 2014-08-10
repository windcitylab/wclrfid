//
//  WCLViewController.m
//  WCLRFID
//
//  Created by Kevin McQuown on 4/20/14.
//  Copyright (c) 2014 Windy City Lab. All rights reserved.
//

#define RBL_SERVICE_UUID @"713D0000-503E-4C75-BA94-3148F18D941E"
#define RBL_CHARACTERISTIC_UUID @"713D0003-503E-4C75-BA94-3148F18D941E"

// Commands going to the Arduino
#define LIST_RECORDS 0x01
#define ADD_RECORD 0x02
#define DELETE_RECORD 0x03
#define READ_RFID_TAG 0x04
#define ENTER_COMMAND_MODE 0x05
#define ENTER_NORMAL_OPERATION_MODE 0x06
#define SENDING_ENTERING_NORMAL_OPERATION_MODE 0x07
#define OPEN_BOLT 0x08
#define CLOSE_BOLT 0x09
#define RESET_COMMAND 0x0A
#define STATUS_CHECK 0xFF

// Messages coming in from the Arduino
#define SENDING_RECORD_COUNT 0x01
#define SENDING_CURRENT_SLOT_NUMBER 0x02
#define SENDING_REFRESH_REQUEST 0x03
#define SENDING_GOOD_TAG_STATUS 0x04
#define SENDING_BAD_TAG_STATUS 0x05
#define SENDING_ENTERING_COMMAND_MODE 0x06

#import "WCLViewController.h"
#import <CoreBluetooth/CoreBluetooth.h>
#import "WCLRFID-Swift.h"

@interface WCLViewController () <CBCentralManagerDelegate, CBPeripheralDelegate, UITableViewDelegate, UITableViewDataSource, UIAlertViewDelegate>

@property (nonatomic, strong) CBCentralManager *centralManager;
@property (nonatomic, strong) CBPeripheral *RFIDReader;
@property (nonatomic, strong) CBUUID *serviceUUID;
@property (nonatomic, strong) CBCharacteristic *notifyOfUpdate;
@property (nonatomic, strong) CBCharacteristic *writeWithoutResponse;
@property (weak, nonatomic) IBOutlet UITableView *tableView;
@property (nonatomic) NSInteger recordCount;
@property (nonatomic, strong) NSMutableArray *names;
@property (nonatomic) BOOL receivingList;
@property (nonatomic) BOOL receivingGoodTagStatus;
@property (nonatomic) BOOL receivingBadTagStatus;
@property (nonatomic, strong) NSData *rfidTagID;
@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) UIAlertView *waitingAlert;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *enterNormalOperationModeButton;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *enterCommandModeButton;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *rfidScanButton;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *refreshButton;
@property (weak, nonatomic) IBOutlet UIBarButtonItem *addButton;

@property (nonatomic, strong) NSTimer *timer;

@property (nonatomic, strong) UIView *viewStatus;

@end

@implementation WCLViewController


- (void) alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
    if (alertView.tag != 99) {
        if (buttonIndex == 1) {
            int byte[1]={0x02};
            NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
            NSString *name = [[alertView textFieldAtIndex:0] text];
            [d appendData:[name dataUsingEncoding:NSUTF8StringEncoding]];
            [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
            self.waitingAlert = [[UIAlertView alloc] initWithTitle:@"Scan RFID Card" message:nil delegate:nil cancelButtonTitle:nil otherButtonTitles: nil];
            [self.waitingAlert show];
        }
    }
    else
    {
        switch (buttonIndex) {
            case 1: // Open
            {
                int byte[1]={OPEN_BOLT};
                NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
                [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
            }
                break;
            case 2: // Close
            {
                int byte[1]={CLOSE_BOLT};
                NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
                [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
            }
                break;
            case 3: // Reset
            {
                int byte[1]={RESET_COMMAND};
                NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
                [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
            }

            default:
                break;
        }
    }
}
- (void) requestList
{
    [self.names removeAllObjects];
    [self.tableView reloadData];
    int byte[1]={0x01};
    NSData *d = [NSData dataWithBytes:byte length:1];
    [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
    [self.waitingAlert dismissWithClickedButtonIndex:0 animated:YES];
}
-(void)timerFired:(NSTimer *)timer
{
    NSLog(@"Must be in normal operation mode!");
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:nil message:@"Lock is in normal operation mode." delegate:nil cancelButtonTitle:nil otherButtonTitles:@"OK", nil];
    [alert show];
    [self refreshButtonsToCommandMode:NO];
}


#pragma mark - Button Methods
- (IBAction)RFIDScanButtonTapped:(id)sender {
    int byte[1]={READ_RFID_TAG};
    NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
    [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
    self.waitingAlert = [[UIAlertView alloc] initWithTitle:@"Scan RFID Card" message:nil delegate:nil cancelButtonTitle:nil otherButtonTitles: nil];
    [self.waitingAlert show];
}

- (IBAction)buttonTapped:(id)sender {
    [self requestList];
}
- (IBAction)addButtonTapped:(id)sender {
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Name" message:nil delegate:self cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
    alert.alertViewStyle = UIAlertViewStylePlainTextInput;
    [alert show];
}
- (IBAction)normalOperationModeButtonTapped:(id)sender {
    int byte[1]={ENTER_NORMAL_OPERATION_MODE};
    NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
    [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
    self.waitingAlert = [[UIAlertView alloc] initWithTitle:@"Entering Normal Operation Mode" message:nil delegate:nil cancelButtonTitle:nil otherButtonTitles: nil];
    [self.waitingAlert show];
}
- (IBAction)enterCommandModeButtonTapped:(id)sender {
    int byte[1]={ENTER_COMMAND_MODE};
    NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
    [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
    self.waitingAlert = [[UIAlertView alloc] initWithTitle:@"Entering Command Operation Mode.  You will need to scan a tag to bump it into command mode." message:nil delegate:nil cancelButtonTitle:nil otherButtonTitles: nil];
    [self.waitingAlert show];
}
-(void)refreshButtonsToCommandMode:(BOOL)inCommandMode
{
    if (inCommandMode) {
        self.enterNormalOperationModeButton.enabled = YES;
        self.enterCommandModeButton.enabled = NO;
        self.refreshButton.enabled = YES;
        self.rfidScanButton.enabled = YES;
        self.addButton.enabled = YES;
    }
    else {
        self.enterNormalOperationModeButton.enabled = NO;
        self.enterCommandModeButton.enabled = YES;
        self.refreshButton.enabled = NO;
        self.rfidScanButton.enabled = NO;
        self.addButton.enabled = NO;
    }
}

- (IBAction)actionButtonTapped:(id)sender {

    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Lock Operation" message:nil delegate:nil cancelButtonTitle:@"Cancel" otherButtonTitles:@"Open",@"Close", @"Reset",nil];
    alert.tag = 99;
    alert.delegate = self;
    [alert show];
}

#pragma mark - tableview delegates
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return self.names.count;
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"basicCell"];
    cell.textLabel.text = self.names[indexPath.row][@"name"];
    cell.detailTextLabel.text = [self.names[indexPath.row][@"rfidTag"] description];
    return cell;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        //[self.names removeObjectAtIndex:indexPath.row];
        unsigned char command[1];
        command[0] = DELETE_RECORD;
        NSMutableData *data = [NSMutableData dataWithBytes:command length:1];
        [data appendData:self.names[indexPath.row][@"slotNumber"]];
        [self.RFIDReader writeValue:data forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
    }
}

#pragma mark - peripheral delegates
- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    NSData *data = characteristic.value;
    NSLog(@"Receiving: %@",data);
    if (data.length == 1)
    {
        unsigned char bytes[1];
        [data getBytes:bytes length:1];
        if (bytes[0] == SENDING_REFRESH_REQUEST) {
            [self requestList];
        }
        if (bytes[0] == SENDING_GOOD_TAG_STATUS) {
            self.receivingGoodTagStatus = YES;
            [self.waitingAlert dismissWithClickedButtonIndex:0 animated:YES];
        }
        if (bytes[0] == SENDING_BAD_TAG_STATUS) {
            self.receivingBadTagStatus = YES;
            [self.waitingAlert dismissWithClickedButtonIndex:0 animated:YES];
        }
        if (bytes[0] == SENDING_ENTERING_COMMAND_MODE) {
            [self.waitingAlert dismissWithClickedButtonIndex:0 animated:YES];
            [self refreshButtonsToCommandMode:YES];
        }
        if (bytes[0] == SENDING_ENTERING_NORMAL_OPERATION_MODE) {
            [self.waitingAlert dismissWithClickedButtonIndex:0 animated:YES];
            [self refreshButtonsToCommandMode:NO];
        }
        if (bytes[0] == STATUS_CHECK) {
            [self.timer invalidate];
            self.timer = nil;
            [self refreshButtonsToCommandMode:YES];
        }
    }
    if (data.length == 2) {
        unsigned char bytes[2];
        [data getBytes:bytes length:2];
        if (bytes[0] == SENDING_RECORD_COUNT) {
            [self.names removeAllObjects];
            self.recordCount = bytes[1];
            self.receivingList = YES;
        }
        if (bytes[0] == SENDING_CURRENT_SLOT_NUMBER) {
            unsigned char slot[1];
            slot[0] = bytes[1];
            NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
            [self.names addObject:dictionary];
            NSData *d = [NSData dataWithBytes:slot length:1];
            [dictionary setObject:d forKey:@"slotNumber"];
        }
    }
    if (data.length == 4) {
        if (self.receivingGoodTagStatus | self.receivingBadTagStatus) {
            self.rfidTagID = data;
            if (self.receivingBadTagStatus) {
                NSString *response =[NSString stringWithFormat:@"tag %@ is not valid!", self.rfidTagID];
                UIAlertView *alert = [[UIAlertView alloc] initWithTitle:nil message:response delegate:nil cancelButtonTitle:nil otherButtonTitles:@"Bummer!", nil];
                [alert show];
                self.receivingBadTagStatus = NO;
            }
        }
        else {
            NSMutableDictionary *dictionary = self.names.lastObject;
            [dictionary setObject:data forKey:@"rfidTag"];
        }
    }
    if (data.length >= 20) {
        if (self.receivingList) {
            NSString *name = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            NSMutableDictionary *dictionary = self.names.lastObject;
            dictionary[@"name"] = name;
            self.recordCount--;
            if (self.recordCount == 0) {
                self.receivingList = NO;
            }
            [self.tableView reloadData];
        }
        if (self.receivingGoodTagStatus)
        {
            self.name = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            NSString *response =[NSString stringWithFormat:@"Tag %@ for %@ is authorized.",self.rfidTagID,self.name];
            UIAlertView *alert = [[UIAlertView alloc] initWithTitle:response message:nil delegate:nil cancelButtonTitle:nil otherButtonTitles:@"Hooray!", nil];
            [alert show];
            self.receivingGoodTagStatus = NO;
        }
    }
}
- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    if (!error) {
        
    }
}
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error
{
    for (CBCharacteristic *c in service.characteristics)
    {
        if (c.properties == CBCharacteristicPropertyNotify) {
            [self.RFIDReader setNotifyValue:YES forCharacteristic:c];
            self.notifyOfUpdate = c;
        }
        if (c.properties == CBCharacteristicPropertyWriteWithoutResponse) {
            self.writeWithoutResponse = c;
            int byte[1]={STATUS_CHECK};
            NSMutableData *d = [NSMutableData dataWithBytes:byte length:1];
            [self.RFIDReader writeValue:d forCharacteristic:self.writeWithoutResponse type:CBCharacteristicWriteWithoutResponse];
            self.timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(timerFired:) userInfo:nil repeats:NO];
        }
    }
}


- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
    [self.RFIDReader discoverCharacteristics:nil forService:peripheral.services.firstObject];
}
#pragma mark - central manager delegates

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral
{
    self.RFIDReader.delegate = self;
    [self.RFIDReader discoverServices:nil];
    
}
- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary *)advertisementData RSSI:(NSNumber *)RSSI
{
    self.viewStatus.backgroundColor = [UIColor blueColor];
    self.RFIDReader = peripheral;
    [self.centralManager connectPeripheral:self.RFIDReader options:nil];
    
}
- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    self.viewStatus.backgroundColor = [UIColor yellowColor];
    [self.centralManager scanForPeripheralsWithServices:@[self.serviceUUID] options:nil];
}
- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    switch (central.state) {
        case CBCentralManagerStatePoweredOn:
        {
            self.viewStatus.layer.borderColor = [[UIColor blackColor] CGColor];
            self.viewStatus.layer.borderWidth = 2;
            self.serviceUUID = [CBUUID UUIDWithString:RBL_SERVICE_UUID];
            [central scanForPeripheralsWithServices:@[self.serviceUUID]  options:nil];
        }
            break;
            
        default:
            break;
    }
}

#pragma mark - view lifecycle
- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
}
- (void)viewDidLoad
{
    [super viewDidLoad];
    self.centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    self.names = [NSMutableArray array];
    self.receivingList = NO;
    [self refreshButtonsToCommandMode:YES];
    WCLConstants *constant;
    

//    self.viewStatus = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 32, 25)];
//    self.viewStatus.backgroundColor = [UIColor whiteColor];
//    self.navigationItem.titleView = self.viewStatus;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
