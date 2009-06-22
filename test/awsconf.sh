#!/bin/sh

cat << EOF
paxos.nodeID = $1
paxos.endpoints = 10.226.114.36:10000, 10.226.101.171:10000, 10.226.86.147:10000

io.maxfd = 4096

database.dir = test/$1
database.checkpointSize = 100000
database.checkpointInterval = 60
database.pageSize = 4096
database.cacheSize = 200M
database.logBufferSize = 10M

asyncDatabase.numReader = 6

http.port = 8080
memcached.port = 11110
keyspace.port = 7080

log.timestamping = true
log.trace = true
log.targets = stdout, file
log.file = test/$1/keyspace.log

timecheck.active = true
EOF