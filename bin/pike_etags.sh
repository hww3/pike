#!/bin/sh
#
# $Id: pike_etags.sh,v 1.1 2003/01/31 22:04:13 grubba Exp $
#
# Run etags with regexps suitable for Pike files.
#
exec etags -l none \
  -r '/[ \t]*\(\<\(public\|inline\|final\|static\|protected\|local\|optional\|private\|nomask\|variant\)\>[ \t]\{1,\}\)*\(\<class\>\)[ \t]\{1,\}\<\([a-zA-Z�-�_][a-zA-Z�-�_0-9]*\)\>/\4/' \
  -r '/[ \t]*\(\<\(mixed\|float\|int\|program\|string\|function\|function(.*)\|array\|array(.*)\|mapping\|mapping(.*)\|multiset\|multiset(.*)\|object\|object(.*)\|void\|constant\|class\)\>)*\|\<\([A-Z�-��-�][a-zA-Z�-�_0-9]*\)\>\)[ \t]\{1,\}\(\<\([_a-zA-Z�-�][_a-zA-Z�-�0-9]*\)\>\|``?\(!=\|->=?\|<[<=]\|==\|>[=>]\|\[\]=?\|()\|[%-!^&+*<>|~\/]\)\)[ \t]*(/\4/' \
  -r '/#[ \t]*define[ \t]+\([_a-zA-Z]+\)(?/\1/' "$@"
