# !/bin/bash
function ch_ff()
{
    cd "$1"
    for file in *
        do
            if [ -f "$file" ]
            then
				dos2unix $file > /dev/null 2>&1
            fi
            if [ -d "$file" ]
            then
                (ch_ff "$PWD/$file")
            fi
        done
}
path=`dirname $0`"/.."
ch_ff $path