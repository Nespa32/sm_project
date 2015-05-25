
while read -r line
do
    filename=$line
    output_file=fixed_$line
    sox $line -r 16000 -c 1 -q $output_file
    echo $output_file
done
