#!/bin/sh

sed -e "s@ $1/\([-a-zA-Z0-9.,_]*\)@ \$(SRCDIR)/\1@g" >$1/dependencies

#sed -e "s@/./@/@g
#s@$1/\([-a-zA-Z0-9.,_]*\)@\$(SRCDIR)/\1@g" >$1/dependencies

# sed "s@[-/a-zA-Z0-9.,_][-/a-zA-Z0-9.,_]*/\([-a-zA-Z0-9.,_]*\)@\1@g" > $1/dependencies

