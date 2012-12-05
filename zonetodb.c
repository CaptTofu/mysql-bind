/*
 * MySQL BIND SDB Driver zone to db conversion utility
 *
 * Copyright (C) 2003-2004 Robert Prouse <mysqlbind@prouse.org>.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA 02111-1307 USA
 *
 * $Id: zonetodb.c,v 1.1.1.1 2004/03/10 04:15:40 alteridem Exp $ 
 */
#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#include <mysql.h>

/*
 * This file is a modification of the PostGreSQL version which is distributed
 * in the contrib/sdb/pgsql/ directory of the BIND 9.2.2 source code,
 * modified by Robert Prouse <mysqlbind@prouse.org>.
 *
 * Generates a MySQL table from a zone.
 *
 * This program is untested and very alpha.  It may destroy data, so use with
 * caution.  I prefer to populate my db's by hand and I recommend you do the
 * same.
 *
 * This is compiled this with something like the following (assuming bind9 has
 * been installed):
 *
 * gcc -g `isc-config.sh --cflags isc dns` -I'/usr/include/mysql' -c zonetodb.c
 * gcc -g -o zonetodb zonetodb.o `isc-config.sh --libs isc dns` -L'/usr/lib/mysql' -lmysqlclient -lz -lcrypt -lnsl -lm -lc -lnss_files -lnss_dns -lresolv -lc -lnss_files -lnss_dns -lresolv
 */

MYSQL conn;
char *dbname, *dbtable;
char str[10240];

void closeandexit(int status)
{
    mysql_close(&conn);
    exit(status);
}

void check_result(isc_result_t result, const char *message)
{
    if (result != ISC_R_SUCCESS)
    {
        fprintf(stderr, "%s: %s\n", message, isc_result_totext(result));
        closeandexit(1);
    }
}

/*
 * Canonicalize a string before writing it to the database.
 * "dest" must be an array of at least size 2*strlen(source) + 1.
 */
static void quotestring(const char *source, char *dest)
{
    while (*source != 0)
    {
        if (*source == '\'')
            *dest++ = '\'';
        else if (*source == '\\')
            *dest++ = '\\';
        *dest++ = *source++;
    }
    *dest++ = 0;
}

void addrdata(dns_name_t *name, dns_ttl_t ttl, dns_rdata_t *rdata)
{
    unsigned char namearray[DNS_NAME_MAXTEXT + 1];
    unsigned char canonnamearray[2 * DNS_NAME_MAXTEXT + 1];
    unsigned char typearray[20];
    unsigned char canontypearray[40];
    unsigned char dataarray[2048];
    unsigned char canondataarray[4096];
    isc_buffer_t b;
    isc_result_t result;

    isc_buffer_init(&b, namearray, sizeof(namearray) - 1);
    result = dns_name_totext(name, ISC_TRUE, &b);
    check_result(result, "dns_name_totext");
    namearray[isc_buffer_usedlength(&b)] = 0;
    quotestring(namearray, canonnamearray);

    isc_buffer_init(&b, typearray, sizeof(typearray) - 1);
    result = dns_rdatatype_totext(rdata->type, &b);
    check_result(result, "dns_rdatatype_totext");
    typearray[isc_buffer_usedlength(&b)] = 0;
    quotestring(typearray, canontypearray);

    isc_buffer_init(&b, dataarray, sizeof(dataarray) - 1);
    result = dns_rdata_totext(rdata, NULL, &b);
    check_result(result, "dns_rdata_totext");
    dataarray[isc_buffer_usedlength(&b)] = 0;
    quotestring(dataarray, canondataarray);

    snprintf(str, sizeof(str),
            "INSERT INTO %s (name, ttl, rdtype, rdata)"
            " VALUES ('%s', %d, '%s', '%s')",
            dbtable, canonnamearray, ttl, canontypearray, canondataarray);
    printf("%s\n", str);
    if( mysql_query(&conn, str) != 0 )
    {
        fprintf(stderr, "INSERT INTO command failed: %s\n", mysql_error(&conn));
        closeandexit(1);
    }
}

int main(int argc, char **argv)
{
    char *porigin, *zonefile, *user, *password;
    dns_fixedname_t forigin, fname;
    dns_name_t *origin, *name;
    dns_db_t *db = NULL;
    dns_dbiterator_t *dbiter;
    dns_dbnode_t *node;
    dns_rdatasetiter_t *rdsiter;
    dns_rdataset_t rdataset;
    dns_rdata_t rdata = DNS_RDATA_INIT;
    isc_mem_t *mctx = NULL;
    isc_buffer_t b;
    isc_result_t result;

    if (argc != 7)
    {
        printf("usage: %s origin file dbname dbtable user password\n", argv[0]);
        printf("Note that dbname must be an existing database.\n");
        exit(1);
    }

    porigin  = argv[1];
    zonefile = argv[2];
    dbname   = argv[3];
    dbtable  = argv[4];
    user     = argv[5];
    password = argv[6];

    dns_result_register();
                
    mctx   = NULL;
    result = isc_mem_create(0, 0, &mctx);
    check_result(result, "isc_mem_create");

    isc_buffer_init(&b, porigin, strlen(porigin));
    isc_buffer_add(&b, strlen(porigin));
    dns_fixedname_init(&forigin);
    origin = dns_fixedname_name(&forigin);
    result = dns_name_fromtext(origin, &b, dns_rootname, ISC_FALSE, NULL);
    check_result(result, "dns_name_fromtext");
        
    db = NULL;
    result = dns_db_create(mctx, "rbt", origin, dns_dbtype_zone,
                             dns_rdataclass_in, 0, NULL, &db);
    check_result(result, "dns_db_create");

    result = dns_db_load(db, zonefile);
    if (result == DNS_R_SEENINCLUDE)
    	result = ISC_R_SUCCESS;
    check_result(result, "dns_db_load");

    printf("Connecting to '%s'\n", dbname);  

    if(!mysql_init(&conn) ||
       !mysql_real_connect(&conn, "localhost", user, password, dbname, 0, NULL, 0))
    {
    	fprintf(stderr, "Connection to database '%s' failed: %s\n",
    		dbname, mysql_error(&conn));
    	closeandexit(1);
    }

    snprintf(str, sizeof(str), "DROP TABLE %s", dbtable);
    printf("%s\n", str);
    if( mysql_query(&conn, str) != 0 )
    {
        fprintf(stderr, "DROP TABLE command failed: %s\n", mysql_error(&conn));
    }
    
    snprintf(str, sizeof(str),
	 "CREATE TABLE %s "
	 "(name VARCHAR(255), ttl INT, rdtype VARCHAR(255), rdata VARCHAR(255)",
	 dbtable);
    printf("%s\n", str);
    if( mysql_query(&conn, str) != 0 )
    {
        fprintf(stderr, "CREATE TABLE command failed: %s\n", mysql_error(&conn));
        closeandexit(1);
    }

    dbiter = NULL;
    result = dns_db_createiterator(db, ISC_FALSE, &dbiter);
    check_result(result, "dns_db_createiterator()");

    result = dns_dbiterator_first(dbiter);
    check_result(result, "dns_dbiterator_first");

    dns_fixedname_init(&fname);
    name = dns_fixedname_name(&fname);
    dns_rdataset_init(&rdataset);
    dns_rdata_init(&rdata);

    while (result == ISC_R_SUCCESS)
    {
        node = NULL;
        result = dns_dbiterator_current(dbiter, &node, name);
        if (result == ISC_R_NOMORE)
            break;
        check_result(result, "dns_dbiterator_current");

        rdsiter = NULL;
        result = dns_db_allrdatasets(db, node, NULL, 0, &rdsiter);
        check_result(result, "dns_db_allrdatasets");

        result = dns_rdatasetiter_first(rdsiter);

        while (result == ISC_R_SUCCESS)
        {
            dns_rdatasetiter_current(rdsiter, &rdataset);
            result = dns_rdataset_first(&rdataset);
            check_result(result, "dns_rdataset_first");
            while (result == ISC_R_SUCCESS)
            {
                dns_rdataset_current(&rdataset, &rdata);
                addrdata(name, rdataset.ttl, &rdata);
                dns_rdata_reset(&rdata);
                result = dns_rdataset_next(&rdataset);
            }
            dns_rdataset_disassociate(&rdataset);
            result = dns_rdatasetiter_next(rdsiter);
        }
        dns_rdatasetiter_destroy(&rdsiter);
        dns_db_detachnode(db, &node);
        result = dns_dbiterator_next(dbiter);
    }
        
    dns_dbiterator_destroy(&dbiter);
    dns_db_detach(&db);
    isc_mem_destroy(&mctx);
    closeandexit(0);
    exit(0);
}
