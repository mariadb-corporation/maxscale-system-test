#!/bin/bash

export test_name=bug562
$test_dir/configure_maxscale.sh

mariadb_err=`mysql -u no_such_user -psome_pwd -h $repl_001 test 2>&1`
maxscale_err=`mysql -u no_such_user -psome_pwd -h $maxscale_IP -P 4006 test 2>&1`

echo "MariaDB message"
echo "$mariadb_err"
echo " "
echo "Maxscale message"
echo "$maxscale_err"


echo "$maxscale_err" | grep "$mariadb_err"
if [ "$?" != 0 ]; then
	echo "Messages are different!"
	res=1
else
	echo "Messages are same"
	res=0
fi

$test_dir/copy_logs.sh bug562
exit $res