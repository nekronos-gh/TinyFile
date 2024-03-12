# Project 2: Inter Process Communication
## TinyFile


TinyFile service
--n_sms: # shared memory segments
--sms_size: the size of shared memory segments, in the unit of bytes.
--qos: use quality of service

e.g. running TinyFile service in blocking mode: ./tinyfile --n_sms 5 --sms_size 32 --qos


Sample app
--state: SYNC | ASYNC
--file: specify the file path to be compressed
--files: specify the file containing the list of files to compressed. You will want to take advantage of this argument to test QoS.

e.g. ./sample_app --file ./aos_is_fun.txt --state SYNC
