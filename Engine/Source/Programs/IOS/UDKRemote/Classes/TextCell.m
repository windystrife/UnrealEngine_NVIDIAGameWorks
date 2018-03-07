// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
//  TextCell.m
//  UDKRemote
//
//  Created by jadams on 8/2/10.
//

#import "TextCell.h"


@implementation TextCell

@synthesize TextField;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if ((self = [super initWithStyle:style reuseIdentifier:reuseIdentifier])) {
        // Initialization code
    }
    return self;
}

/**
 * When the cell is clicked, make the text field enter edit mode
 */
- (void)setSelected:(BOOL)selected animated:(BOOL)animated 
{
    [super setSelected:selected animated:animated];

	if (selected)
	{
		[TextField becomeFirstResponder];
	}
}


- (void)dealloc {
    [super dealloc];
}


@end
