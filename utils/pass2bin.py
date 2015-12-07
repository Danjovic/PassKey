#############################################################
# KeyPass - Password list to hex file converter
# Author: Daniel Jose Viana - danjovic@hotmail.com
#
# Version 0.1 - December 2, 2015
# Basic Release
#
# Version 0.11 - December 7, 2015
# Added support for <ENTER> at the end of password
#
# This code is released under GPL V2.0
# 
# This script reads a text file with your legacy passwords
# and generates an Hex file with the password contents and
# a command line with arguments for avrdude to upload them
# to KeyPass.
#
# Read 'password_rules.txt' for more info.
#

## FUNCTIONS ##
def pad_with_zeroes(s):  # Keypass reserves 14 bytes for each password
	s=s+ ''.join(chr(0) for x in range (0,14))
	return s[0:14]


## MAIN CODE ##

passwords=['\0','\0','\0','\0','\0','\0','\0','\0','\0','\0']


try:
    fi=open("passwords.txt", "r")
    print "Found password file. Parsing"
    passes = 0   # passwords found
    seeking = 1
    cr_at_end = 0 
    while seeking:
        line = fi.readline()  # discard lines up to first %%% found  
        if not line: break
        if line.count("%%%")!=0: seeking =0 # found start mark
    print "Seeking passwords"
    
    while passes < 10:
        line = fi.readline()
        if not line: break
        line = line.strip('\n') #strip the end of line
        linesize = len(line)
        if linesize>12 and line[12]=="#":
                cr_at_end = 1
        else: cr_at_end =0
        
        line = line[0:12]       # limit the number of characters
        if linesize>12:
            line=line.rstrip()  # trim spaces for lines with comments
        if len(line)!=0:
            if cr_at_end: line = line + chr(10)
            passwords[passes]=line
            passes=passes+1              
    fi.close()                  # Reached End of file of found all passwords

    
    
    if passes!=0:
        print "Passwords found: %d" % passes
        print "Generating eeprom.bin file"

        eeprom_image=''             # initialize eeprom image
        for i in range (0, passes): # fill with found passwords
            eeprom_image=eeprom_image+pad_with_zeroes(passwords[i])
        for i in range (passes,10): # pad not found passwords
            eeprom_image=eeprom_image+pad_with_zeroes('')

        # complete bin file up to 512 bytes 
        eeprom_image = eeprom_image + ''.join(chr(255) for x in range (140,512))

        try:
            fo=open("eeprom.bin", "wb")  # open file for write
            fo.write(eeprom_image)
            fo.close()

            print "Password bin file generated sucessfully"
            print 
            print "Now put Keypass in programming mode and type:"
            print "avrdude -p m8 -c usbasp -U eeprom:w:eeprom.bin"
            
            
            
        except IOError:
            print "Error: Could not create eeprom.bin output file"

        
    else :
        print "No passwords found. Nothing to do"
        

    
except IOError:
    print "Error: File passwords.txt not found."





