 #!/bin/bash

# Do not repeat latency benchmark.
((REP = 1))

#Number of witnesses.
SIZES=$(seq 2 -1 0)

#eval $1 | tee "$LOGFILE"

OUTPUTPREFIX=$1"-batch"

#mkdir /shome/resultRedis

for SIZE in $SIZES; do
#  for CLISIZE in $(seq $SIZE -4 3); do
#    ((CLISIZE = 20))
    echo "witness: $SIZE outputFile: /shome/resultRedis/writeDistRandom-witness$SIZE.raw"

    for ITER in $(seq $REP -1 1); do
#        LOGFILE="$(date +%Y%m%d%H%M%S)_writeDistRandom.raw"
#        ./redis_benchmark writeDistRandom --count 1000000 --witness $SIZE > /tmp/$LOGFILE
        ./redis_benchmark writeDistRandom --count 1000000 --witness $SIZE > /shome/resultRedis/writeDistRandom-witness$SIZE.raw
    done
    sleep 4
#  done
done

#sleep 10
#./scripts/parseWriteThroughputByBatchSize.py $LOGFILE
