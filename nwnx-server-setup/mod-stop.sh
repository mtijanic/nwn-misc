#!/bin/bash

echo "Shutting down the server"
cat ~/.modpid | xargs kill -9
