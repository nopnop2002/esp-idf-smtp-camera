#!/bin/bash
#
# Extract the root certificate from smtp.googlemail.com.

#set -x

openssl s_client -showcerts -connect smtp.googlemail.com:587 -starttls smtp </dev/null >hoge

start=`grep -e "-----BEGIN CERTIFICATE-----" -n hoge | sed -e 's/:.*//g' | tail -n 1`

last=`grep -e "-----END CERTIFICATE-----" -n hoge | sed -e 's/:.*//g' | tail -n 1`

sed -n ${start},${last}p hoge > main/gmail_root_cert.pem

rm hoge
