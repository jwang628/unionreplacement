# unionreplacement Makefile

MODULE_big = unionreplacement
OBJS = unionreplacement.o ur.o
#BJS = unionreplacement.o 
#MODULES = unionreplacement 
DATA = unionreplacement--0.0.1.sql
#BJS = unionreplacement.o ur.o
EXTENSION = unionreplacement
PGFILEDESC = "unionreplacement - translate SQL statements"


#REGRESS_OPTS = --temp-instance=/tmp/5454 --port=5454 --temp-config unionreplacement.conf
#REGRESS=test0 test1 test2 test3 test4 test5 test6 test7

PG_CONFIG = /usr/pgsql-14/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

#
#pgxn:
#git archive --format zip  --output ../pgxn/unionreplacement/unionreplacement-0.0.4.zip master
