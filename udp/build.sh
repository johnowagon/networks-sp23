#!/bin/bash

touch testfile.txt
for ((i = 0 ; i < 6000000 ; i++)); do
  echo "$i" >> testfile.txt
done
