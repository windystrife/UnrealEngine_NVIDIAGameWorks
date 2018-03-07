!/bin/bash
     for File in *.c; do
     mv $File $File.old
     sed 's/<libelf.h>/"libelf.h"/g' $File.old > $File
     rm -f $File.old
     done

#!/bin/bash
     for File in *.c; do
     mv $File $File.old
     sed 's/<gelf.h>/"gelf.h"/g' $File.old > $File
     rm -f $File.old
     done

#!/bin/bash
     for File in *.h; do
     mv $File $File.old
     sed 's/<gelf.h>/"gelf.h"/g' $File.old > $File
     rm -f $File.old
     done
