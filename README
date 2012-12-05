BUILDING THE BIND SERVER
========================

The driver module and header file (mysqldb.c and mysqldb.h) must be copied to (or linked into) the bind9/bin/named and bind9/bin/named/include directories respectively, 

In File bin/named/Makefile.in
set DBDRIVER_OBJS = mysqldb.@O@
set DBDRIVER_SRCS = mysqldb.c

Add the results from the command mysql_config --cflags to DBDRIVER_INCLUDES.
(e.g. DBDRIVER_INCLUDES = -I'/usr/include/mysql')

Add the results from the command mysql_config --libs to DBRIVER_LIBS.
(e.g. DBDRIVER_LIBS = -L'/usr/lib/mysql' -lmysqlclient -lz -lcrypt -lnsl -lm -lc -lnss_files -lnss_dns -lresolv -lc -lnss_files -lnss_dns -lresolv)

In bind9/bin/named/main.c, add an include to mysqldb.h (e.g. #include "mysqldb.h") Then you must register the driver in setup(), by adding mysqldb_init(); before the call to ns_server_create().

Unregistration should be in cleanup(), by adding the call mysqldb_clear(); after the call to ns_server_destroy().

./configure
make
make install

You should read the README file with the BIND distribution for more options to pass to configure. You may also want to read the document sdb in the bind doc/misc/ directory.

ENTRY IN NAMED.CONF
===================

zone "mydomain.com" {
  type master;
  notify no;
  database "mysqldb dbname tablename hostname user password";
};

In the first line, change the zone to match your domain. In the database line, dbname is the name of the MySQL database, tablename is the name of the table for this domain/zone, hostname is the name of the database host, user and password are for access to the database.

DATABASE SCHEMA
===============

You should create a database for the driver and add one table for each domain. The following SQL will create a table for one domain. Change mydomain to something that will represent the domain more clearly.

CREATE TABLE mydomain (
  name varchar(255) default NULL,
  ttl int(11) default NULL,
  rdtype varchar(255) default NULL,
  rdata varchar(255) default NULL
) TYPE=MyISAM;

EXAMPLE DATABASE DATA
=====================

The following is an SQL dump of an example table for a domain. The entries should be familiar to anyone who is familiar with BIND zone files. Change it to match your domain.

INSERT INTO mydomain VALUES ('mydomain.com', 259200, 'SOA', 'mydomain.com. www.mydomain.com. 200309181 28800 7200 86400 28800');
INSERT INTO mydomain VALUES ('mydomain.com', 259200, 'NS', 'ns0.mydomain.com.');
INSERT INTO mydomain VALUES ('mydomain.com', 259200, 'NS', 'ns1.mydomain.com.');
INSERT INTO mydomain VALUES ('mydomain.com', 259200, 'MX', '10 mail.mydomain.com.');
INSERT INTO mydomain VALUES ('w0.mydomain.com', 259200, 'A', '192.168.1.1');
INSERT INTO mydomain VALUES ('w1.mydomain.com', 259200, 'A', '192.168.1.2');
INSERT INTO mydomain VALUES ('mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
INSERT INTO mydomain VALUES ('mail.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
INSERT INTO mydomain VALUES ('ns0.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
INSERT INTO mydomain VALUES ('ns1.mydomain.com', 259200, 'Cname', 'w1.mydomain.com.');
INSERT INTO mydomain VALUES ('www.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
INSERT INTO mydomain VALUES ('ftp.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
