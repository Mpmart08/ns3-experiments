#!/bin/bash
list="TcpNewReno TcpHybla TcpHighSpeed TcpHtcp TcpVegas TcpScalable TcpVeno TcpBic TcpYeah TcpIllinois TcpWestwood TcpWestwoodPlus"
for item in $list
do
    for i in 1 4 8 16 32
    do
        cd "/home/workspace/ns-allinone-3.26/ns-3.26/"
        ./waf --run "p1 --nSpokes=$i --protocol=$item" > "/home/workspace/ns-allinone-3.26/ns-3.26/scratch/$item$i.txt"
    done
done
