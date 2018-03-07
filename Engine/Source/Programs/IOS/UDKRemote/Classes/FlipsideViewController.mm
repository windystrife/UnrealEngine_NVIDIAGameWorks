// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
//  FlipsideViewController.m
//  UDKRemote
//
//  Created by jadams on 7/28/10.
//

#import "FlipsideViewController.h"
#import "UDKRemoteAppDelegate.h"
#import "MainViewController.h"

@implementation FlipsideViewController

//@synthesize my_delegate;
@synthesize DestIPCell;
@synthesize PortCell;
@synthesize TiltCell;
@synthesize TouchCell;
@synthesize LockCell;
@synthesize SubIPTextField;
@synthesize PortTextField;
@synthesize SubTiltSwitch;
@synthesize SubTouchSwitch;
@synthesize SubLockSwitch;
@synthesize MainSettingsTable;
@synthesize RecentComputersTable;
@synthesize RecentComputersController;
@synthesize ComputerListEdit;
@synthesize NewComputerTextField;
@synthesize TextAlert;

/**
 * Enum to describe each and how many sections are in the table
 */
enum ESections
{
	SECTION_Settings = 0,
	SECTION_MAX
};

/**
 * Enum to describe each and how many rows in section 0
 */
enum ESection0Rows
{
	SECTION0_IPAddr = 0,
	SECTION0_Port,
	SECTION0_Tilt,
	SECTION0_Touch,
	SECTION0_MAX_IPad,
	SECTION0_Lock=SECTION0_MAX_IPad,
	SECTION0_MAX
};


- (void)viewDidLoad
{
    [super viewDidLoad];
	
	AppDelegate = ((UDKRemoteAppDelegate*)[UIApplication sharedApplication].delegate);
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
	
	// get settings from app
	self.SubIPTextField.text = AppDelegate.PCAddress;
	self.PortTextField.text = [NSString stringWithFormat:@"%d", AppDelegate.Port];
	self.SubTiltSwitch.on = !AppDelegate.bShouldIgnoreTilt;
	self.SubTouchSwitch.on = !AppDelegate.bShouldIgnoreTouch;
	self.SubLockSwitch.on = AppDelegate.bLockOrientation;
	
	// make sure toolbar from adding comptuer page is hidden
	[self setToolbarHidden:YES];
}


- (IBAction)done 
{
	// write settings back to app
	AppDelegate.Port = [self.PortTextField.text intValue];
	AppDelegate.bShouldIgnoreTilt = !self.SubTiltSwitch.on;
	AppDelegate.bShouldIgnoreTouch = !self.SubTouchSwitch.on;
	AppDelegate.bLockOrientation = self.SubLockSwitch.on;
	if (AppDelegate.bLockOrientation)
	{
		AppDelegate.LockedOrientation = self.interfaceOrientation;
	}

	// always save the tilt setting
	[[NSUserDefaults standardUserDefaults] setObject:AppDelegate.PCAddress forKey:@"PCAddress"];
	[[NSUserDefaults standardUserDefaults] setBool:AppDelegate.bShouldIgnoreTilt forKey:@"bShouldIgnoreTilt"];
	[[NSUserDefaults standardUserDefaults] setBool:AppDelegate.bShouldIgnoreTouch forKey:@"bShouldIgnoreTouch"];
	[[NSUserDefaults standardUserDefaults] setBool:AppDelegate.bLockOrientation forKey:@"bLockOrientation"];
	[[NSUserDefaults standardUserDefaults] setInteger:(int)AppDelegate.LockedOrientation forKey:@"LockedOrientation"];
	[[NSUserDefaults standardUserDefaults] setInteger:AppDelegate.Port forKey:@"Port"];
	[[NSUserDefaults standardUserDefaults] setObject:AppDelegate.RecentComputers forKey:@"RecentComputers"];
	[[NSUserDefaults standardUserDefaults] synchronize];

    MainViewController* MainThing = (MainViewController*)(self.delegate);
    
	[MainThing flipsideViewControllerDidFinish:self];
}

- (IBAction)AddComputer
{ 
	float OSVersion = [[[UIDevice currentDevice] systemVersion] floatValue];
	
	self.TextAlert = [[UIAlertView alloc] initWithTitle:@"Add New Computer" 
												message:(OSVersion < 4.0f ? @"\n" : @"\n\n")
											   delegate:self 
									  cancelButtonTitle:NSLocalizedString(@"Cancel", nil) 
									  otherButtonTitles:NSLocalizedString(@"OK", nil), nil];
										  
	UITextField* TextPrompt = [[UITextField alloc] initWithFrame:CGRectMake(16, 40, 252, 25)];
	TextPrompt.font = [UIFont systemFontOfSize:18];
	TextPrompt.keyboardAppearance = UIKeyboardAppearanceAlert;
	TextPrompt.autocorrectionType = UITextAutocorrectionTypeNo;
	TextPrompt.autocapitalizationType = UITextAutocapitalizationTypeNone;
	TextPrompt.borderStyle = UITextBorderStyleRoundedRect;
	TextPrompt.placeholder = @"Computer name or IP";
	TextPrompt.delegate = self;
	[TextPrompt becomeFirstResponder];
	
	// jam the text field into the alert view
	[self.TextAlert addSubview:TextPrompt];
	
	// move up on old OSs
	if (OSVersion < 4.0f)
	{
		self.TextAlert.transform = CGAffineTransformTranslate(self.TextAlert.transform, 0.0, 100.0);
	}
	
	// self.TextAlert the alert
	[self.TextAlert show];
	
	// release our retained items
	[self.TextAlert release];
	[TextPrompt release];
	self.NewComputerTextField = TextPrompt;
}


- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
	if ([[[UIDevice currentDevice] systemVersion] floatValue] < 4.0f)
	{
		self.TextAlert.transform = CGAffineTransformTranslate(self.TextAlert.transform, 0.0, -100.0);
	}
}

/**
 * Handle pressing OK or Return on the keyboard
 */
- (void)ProcessOK
{
	// get the text the user entered
	NSString* EnteredText = self.NewComputerTextField.text;
	
	// check to make sure it's not already there
	bool bAlreadyExists = NO;
	for (NSString* Existing in AppDelegate.RecentComputers)
	{
		if ([Existing compare:EnteredText] == NSOrderedSame)
		{
			bAlreadyExists = YES;
			break;
		}
	}
	
	// add it if it wasn't already there
	if (!bAlreadyExists)
	{
		[AppDelegate.RecentComputers addObject:EnteredText];
	}
	
	// even if it was already there, use it as the current address
	AppDelegate.PCAddress = EnteredText;
	self.SubIPTextField.text = AppDelegate.PCAddress;
	[self.RecentComputersTable reloadData];	
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
	[self.TextAlert dismissWithClickedButtonIndex:1 animated:YES];
	return NO;
}

- (void)alertView:(UIAlertView*)AlertView willDismissWithButtonIndex:(NSInteger)ButtonIndex
{
	// let go of the keyboard before dismissing (to avoid "wait_fences: failed to receive reply: 10004003")
	[self.NewComputerTextField resignFirstResponder];	
}

- (void)alertView:(UIAlertView*)AlertView didDismissWithButtonIndex:(NSInteger)ButtonIndex
{
	// on OK, do something (on cancel, do nothing)
	if (ButtonIndex == 1)
	{
		[self ProcessOK];
	}
	
	self.TextAlert = nil;
	self.NewComputerTextField = nil;
}

- (IBAction)EditList
{
	// toggle editing mode
	self.RecentComputersTable.editing = !self.RecentComputersTable.editing;
	
	if (self.RecentComputersTable.editing)
	{
		[self.ComputerListEdit setTitle:@"Done"];
	}
	else
	{
		[self.ComputerListEdit setTitle:@"Edit"];
		[RecentComputersTable reloadData];
	}
}

- (IBAction)CalibrateTilt
{
	[AppDelegate.mainViewController CalibrateTilt];
}


- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
	return SECTION_MAX;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
	if (tableView == MainSettingsTable)
	{
		// return how many rows for each section
		switch (section)
		{
			case SECTION_Settings:
				return (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) ? SECTION0_MAX_IPad : SECTION0_MAX;
				
				// more sections here
		}		
	}
	else if (tableView == RecentComputersTable)
	{
		return [AppDelegate.RecentComputers count];
	}
	
	return 0;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
	if (tableView == MainSettingsTable)
	{
		// return a unique cell for each row
		switch (indexPath.section)
		{
			case SECTION_Settings:
				switch (indexPath.row)
				{
					case SECTION0_IPAddr:
						return DestIPCell;
					case SECTION0_Port:
						return PortCell;
					case SECTION0_Tilt:
						return TiltCell;
					case SECTION0_Touch:
						return TouchCell;
					case SECTION0_Lock:
						return LockCell;
				}
				break;
				
			// more sections here
		}
	}
	else if (tableView == RecentComputersTable)
	{
		// get the computer name from our list of names
		NSString* ComputerName = [AppDelegate.RecentComputers objectAtIndex:indexPath.row];

		// reuse or create a cell for the name
		UITableViewCell* Cell = [RecentComputersTable dequeueReusableCellWithIdentifier:ComputerName];
		if (Cell == nil)
		{
			Cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:ComputerName] autorelease];
			Cell.textLabel.text = ComputerName;
		}
		Cell.accessoryType = ([AppDelegate.PCAddress compare:ComputerName] == NSOrderedSame) ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
		
		return Cell;
	}
	
	return nil;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
	// if user clicked on the address cell, we need to go to a submenu to handle it
	if (tableView == MainSettingsTable && indexPath.section == SECTION_Settings && indexPath.row == SECTION0_IPAddr)
	{
		[tableView deselectRowAtIndexPath:indexPath animated:YES];
		[self setToolbarHidden:NO animated:NO];
		[self pushViewController:RecentComputersController animated:YES];
		
		// auto prompt for new computer if none exist
		if ([AppDelegate.RecentComputers count] == 0)
		{
			[self AddComputer];
		}
	}
	// if the user clicked on a cell int he computers list, then use that as the current address
	else if (tableView == RecentComputersTable)
	{
		AppDelegate.PCAddress = [AppDelegate.RecentComputers objectAtIndex:indexPath.row];
		self.SubIPTextField.text = AppDelegate.PCAddress;
		[self setToolbarHidden:YES animated:NO];
		[RecentComputersTable reloadData];

		// we can now close this list
		[self popViewControllerAnimated:YES];
	}
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
	return tableView == RecentComputersTable;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
	// delete a row
	if (editingStyle == UITableViewCellEditingStyleDelete)
	{
		// did we delete the selected computer
		NSString* Computer = [AppDelegate.RecentComputers objectAtIndex:indexPath.row];
		bool bRemovedSelected = [Computer compare:AppDelegate.PCAddress] == NSOrderedSame;
		
		// remove the computer from our known list
		[AppDelegate.RecentComputers removeObjectAtIndex:indexPath.row];
		
		// select a computer if we deleted the selected one
		if (bRemovedSelected)
		{
			if ([AppDelegate.RecentComputers count] > 0)
			{
				AppDelegate.PCAddress = [AppDelegate.RecentComputers objectAtIndex:0];
			}
			else 
			{
				AppDelegate.PCAddress = @"";
			}

		}
		
		NSArray* Rows = [NSArray arrayWithObject:indexPath];
		[tableView deleteRowsAtIndexPaths:Rows withRowAnimation:UITableViewRowAnimationBottom];
	}
	
}

- (void)didReceiveMemoryWarning {
	// Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
	// Release any cached data, images, etc that aren't in use.
}


- (void)viewDidUnload 
{
	// Release any retained subviews of the main view.
	self.DestIPCell = nil;
	self.TiltCell = nil;
	self.PortCell = nil;
	self.TouchCell = nil;
	self.LockCell = nil;
}



// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation 
{
	// Return YES for supported orientations
	return YES;//(interfaceOrientation == UIInterfaceOrientationLandscapeRight);
}



- (void)dealloc 
{
    [super dealloc];
}


@end