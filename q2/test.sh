for((i=1;i<=$3;i++))
do
	curl -XGET 'http://127.0.0.1:'$1'/'$2 &
done
