/*
 * MySQL BIND SDB Driver
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
 * $Id: mysqldb.c,v 1.2 2007/11/05 23:16:48 dorgan1983 Exp $ 
 */

#include <config.h>   
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mysql.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/sdb.h>
#include <dns/result.h>

#include <named/globals.h>

/* #include <named/mysqldb.h> */
#include "include/mysqldb.h"

/*
 * This file is a modification of the PostGreSQL version which is distributed
 * in the contrib/sdb/pgsql/ directory of the BIND 9.2.2 source code,
 * modified by Robert Prouse <mysqlbind@prouse.org>
 *
 * A simple database driver that interfaces to a MySQL database.  This
 * is not necessarily complete nor designed for general use, but it is has
 * been in use on production systems for some time without known problems.
 * It opens one connection to the database per zone, which is inefficient.  
 * It also may not handle quoting correctly.
 *
 * The table must contain the fields "name", "type", and "data", and 
 * is expected to contain a properly constructed zone.  
 *
 * The column domain_id is a unique identifyer for a domain, in this case a UUID
 *
 * Example SQL to create a domain
 * ==============================
 *
 * CREATE TABLE mydomain (
 *  id int unsigned not null auto_increment,
 *  domain_id char(32) NOT NULL default '',
 *  name varchar(255) default NULL,
 *  ttl int(11) default NULL,
 *  type varchar(255) default NULL,
 *  data varchar(255) default NULL,
 *  primary key (id),
 *  key domain_id (domain_id),
 *  key name (name)
 * ) TYPE=InnoDB;
 * 
 * INSERT INTO mydomain (domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','mydomain.com', 259200, 'SOA', 'mydomain.com. www.mydomain.com. 200309181 28800 7200 86400 28800');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','mydomain.com', 259200, 'NS', 'ns0.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','mydomain.com', 259200, 'NS', 'ns1.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','mydomain.com', 259200, 'MX', '10 mail.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','w0.mydomain.com', 259200, 'A', '192.168.1.1');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','w1.mydomain.com', 259200, 'A', '192.168.1.2');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','mail.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','ns0.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','ns1.mydomain.com', 259200, 'Cname', 'w1.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','www.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
 * INSERT INTO mydomain(domain_id, name, ttl, type, data) VALUES ('5f7b75e1-8a25-49a5-8ba0-e2efe53aebed','ftp.mydomain.com', 259200, 'Cname', 'w0.mydomain.com.');
 *
 * Example entry in named.conf
 * ===========================
 *
 * zone "mydomain.com" {
 *	type master;
 *	notify no;
 *	database "mysqldb dbname tablename hostname user password domain_id";
 * };
 *
 * Rebuilding the Server (modified from bind9/doc/misc/sdb)
 * =====================================================
 * 
 * The driver module and header file (mysqldb.c and mysqldb.h) 
 * must be copied to (or linked into)
 * the bind9/bin/named and bind9/bin/named/include directories
 * respectively, and must be added to the DBDRIVER_OBJS and DBDRIVER_SRCS
 * lines in bin/named/Makefile.in (e.g. add mysqldb.c to DBDRIVER_SRCS and 
 * mysqldb.@O@ to DBDRIVER_OBJS).  
 * 
 * Add the results from the command `mysql_config --cflags` to DBDRIVER_INCLUDES.
 * (e.g. DBDRIVER_INCLUDES = -I'/usr/include/mysql')
 *
 * Add the results from the command `mysql_config --libs` to DBRIVER_LIBS.
 * (e.g. DBDRIVER_LIBS = -L'/usr/lib/mysql' -lmysqlclient -lz -lcrypt -lnsl -lm -lc -lnss_files -lnss_dns -lresolv -lc -lnss_files -lnss_dns -lresolv)
 * 
 * In bind9/bin/named/main.c, add an include to mysqldb.h 
 * (e.g. #include "mysqldb.h")  Then you must register the driver
 * in setup(), by adding mysqldb_init(); before the call to ns_server_create().  
 * Unregistration should be in cleanup(), by adding the call mysqldb_clear();
 * after the call to ns_server_destroy().
 */

static dns_sdbimplementation_t *mysqldb = NULL;

struct dbinfo
{
    MYSQL conn;
    char *database;
    char *table;
    char *host;
    char *user;
    char *passwd;
    char *domain_id;
    char *tenant_id;
};

static void mysqldb_destroy(const char *zone, void *driverdata, void **dbdata);

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
        /* SQL doesn't treat \ as special, but PostgreSQL does */
        else if (*source == '\\')
            *dest++ = '\\';
        *dest++ = *source++;
    }
    *dest++ = 0;
}

/*
 * Connect to the database.
 */
static isc_result_t db_connect(struct dbinfo *dbi)
{
    if(!mysql_init(&dbi->conn))
        return (ISC_R_FAILURE);

    if (mysql_real_connect(&dbi->conn, dbi->host, dbi->user, dbi->passwd, dbi->database, 0, NULL, 0))
        return (ISC_R_SUCCESS);
    else
        return (ISC_R_FAILURE);
}

/*
 * Check to see if the connection is still valid.  If not, attempt to
 * reconnect.
 */
static isc_result_t maybe_reconnect(struct dbinfo *dbi)
{
    if (!mysql_ping(&dbi->conn))
	return (ISC_R_SUCCESS);

     return (db_connect(dbi));
}

static int  d_ex(char *search, char *domain)
{

  char mdot[]=".";
  char *array[142];
  int loop;

  array[0]=strtok(search,mdot);


   if(array[0]==NULL)
   {

           return 1;

   }

  for(loop=1;loop<142;loop++)
   {
           array[loop]=strtok(NULL,mdot);
           if(array[loop]==NULL)
                   break;
   }

   if(loop<2) {
	return 1;
   }

   snprintf(domain,255,"%s.%s",array[loop-2],array[loop-1]);
   
   return 0;
}

/*
 * This database operates on absolute names.
 *
 * Queries are converted into SQL queries and issued synchronously.  Errors
 * are handled really badly.
 */
 
static isc_result_t mysqldb_lookup(const char *zone, const char *name, void *dbdata,
	                           dns_sdblookup_t *lookup)
{
    isc_result_t result;
    struct dbinfo *dbi = dbdata;
    MYSQL_RES *res = 0;
    MYSQL_ROW row;
    char str[1500];
    //char *canonname;

    UNUSED(zone);

    //canonname = isc_mem_get(ns_g_mctx, strlen(name) * 2 + 1);
    //if (canonname == NULL)
    	//return (ISC_R_NOMEMORY);
    //quotestring(name, canonname);
    snprintf(str, sizeof(str),
             "SELECT ttl, type, data FROM %s WHERE tenant_id = '%s' AND domain_id = '%s' AND name = UPPER('%s') ",
             dbi->table,
             dbi->tenant_id,
             dbi->domain_id,
             name);
    //isc_mem_put(ns_g_mctx, canonname, strlen(name) * 2 + 1);

    result = maybe_reconnect(dbi);
    if (result != ISC_R_SUCCESS)
            return (result);

    if( mysql_query(&dbi->conn, str) != 0 )
    {
            return (ISC_R_FAILURE);
    }
    res = mysql_store_result(&dbi->conn);

    if (mysql_num_rows(res) == 0)
    {
            char domain[255];
            char non_cons_name[255];

            strcpy(non_cons_name,name);

            if(d_ex(non_cons_name,domain) != 0) {
                    return (ISC_R_FAILURE);
            }

            mysql_free_result(res);

            snprintf(str, sizeof(str),
                     "SELECT ttl, type, data FROM %s WHERE tenant_id = '%s' AND domain_id = '%s' AND name = UPPER('*.%s')",
                     dbi->table,
                     dbi->tenant_id,
                     dbi->domain_id,
                     domain);

            result = maybe_reconnect(dbi);
            if (result != ISC_R_SUCCESS)
                    return (result);

            if( mysql_query(&dbi->conn, str) != 0 )
            {
                    return (ISC_R_FAILURE);
            }
            res = mysql_store_result(&dbi->conn);

            if (mysql_num_rows(res) == 0)
            {
                    mysql_free_result(res);
                    return (ISC_R_NOTFOUND);
            }
    }
    while ((row = mysql_fetch_row(res)))
    {
        	char *ttlstr = row[0];
	     	char *type   = row[1];
	     	char *data   = row[2];
	    	dns_ttl_t ttl;
	     	char *endp;
	     	ttl = strtol(ttlstr, &endp, 10);
	     	if (*endp != '\0')
        	{
	             mysql_free_result(res);
        	     return (DNS_R_BADTTL);
	     	}
     		result = dns_sdb_putrr(lookup, type, ttl, data);
	    	if (result != ISC_R_SUCCESS) {
        	     mysql_free_result(res);
	             return (ISC_R_FAILURE);
     		}
	}
	mysql_free_result(res);
	return (ISC_R_SUCCESS);
}

/*
 * Issue an SQL query to return all nodes in the database and fill the
 * allnodes structure.
 */
static isc_result_t mysqldb_allnodes(const char *zone, void *dbdata, dns_sdballnodes_t *allnodes)
{
    struct dbinfo *dbi = dbdata;
    isc_result_t result;
    MYSQL_RES *res = 0;
    MYSQL_ROW row;
    char str[1500];

    UNUSED(zone);

    snprintf(str, sizeof(str),
	"SELECT ttl, name, type, data FROM %s WHERE tenant_id = '%s' AND domain_id = '%s' ORDER BY name",
    dbi->table,
    dbi->tenant_id,
	dbi->domain_id);

    result = maybe_reconnect(dbi);
    if (result != ISC_R_SUCCESS)
	return (result);

    if( mysql_query(&dbi->conn, str) != 0 )
    {
    	return (ISC_R_FAILURE);
    }
    res = mysql_store_result(&dbi->conn);

    if (mysql_num_rows(res) == 0)
    {
    	mysql_free_result(res);
    	return (ISC_R_NOTFOUND);
    }

    while ((row = mysql_fetch_row(res)))
    {
    	char *ttlstr = row[0];
	    char *name   = row[1];
    	char *type   = row[2];
    	char *data   = row[3];
	    dns_ttl_t ttl;
	    char *endp;
	    ttl = strtol(ttlstr, &endp, 10);
	    if (*endp != '\0')
        {
            mysql_free_result(res);
            return (DNS_R_BADTTL);
	    }
	    result = dns_sdb_putnamedrr(allnodes, name, type, ttl, data);
	    if (result != ISC_R_SUCCESS)
        {
            mysql_free_result(res);
            return (ISC_R_FAILURE);
	    }
    }   
    mysql_free_result(res);
    return (ISC_R_SUCCESS);
}

/*
 * Create a connection to the database and save any necessary information
 * in dbdata.
 *
 * argv[0] is the name of the database
 * argv[1] is the name of the table
 * argv[2] (if present) is the name of the host to connect to
 * argv[3] (if present) is the name of the user to connect as
 * argv[4] (if present) is the name of the password to connect with
 * argv[5] (if present) is the domain_id, column specifying zone name (not record name) in the table 
 * argv[6] (if present) is the tenant_id, column specifying owner of records in the table 
 */
static isc_result_t mysqldb_create(const char *zone, int argc, char **argv,
	                           void *driverdata, void **dbdata)
{
    struct dbinfo *dbi;
    isc_result_t result;

    UNUSED(zone);
    UNUSED(driverdata);

    if (argc < 2)
        return (ISC_R_FAILURE);

    dbi = isc_mem_get(ns_g_mctx, sizeof(struct dbinfo));
    if (dbi == NULL)
        return (ISC_R_NOMEMORY);
        
    dbi->database  = NULL;
    dbi->table     = NULL;
    dbi->host      = NULL;
    dbi->user      = NULL;
    dbi->passwd    = NULL;
    dbi->domain_id = NULL;
    dbi->tenant_id = NULL;

#define STRDUP_OR_FAIL(target, source)			\
    do                                                  \
    {							\
	target = isc_mem_strdup(ns_g_mctx, source);	\
	if (target == NULL)                             \
        {				                \
            result = ISC_R_NOMEMORY;		        \
	    goto cleanup;				\
	}						\
    } while (0);

    STRDUP_OR_FAIL(dbi->database,  argv[0]);
    STRDUP_OR_FAIL(dbi->table,     argv[1]);
    if (argc > 2)
        STRDUP_OR_FAIL(dbi->host, argv[2]);
    if (argc > 3)
        STRDUP_OR_FAIL(dbi->user, argv[3]);
    if (argc > 4)
        STRDUP_OR_FAIL(dbi->passwd, argv[4]);
    if (argc > 5)
        STRDUP_OR_FAIL(dbi->domain_id, argv[5]);
    if (argc > 6)
        STRDUP_OR_FAIL(dbi->tenant_id, argv[6]);

    result = db_connect(dbi);
    if (result != ISC_R_SUCCESS)
	goto cleanup;

    *dbdata = dbi;
    return (ISC_R_SUCCESS);

cleanup:
    mysqldb_destroy(zone, driverdata, (void **)&dbi);
    return (result);
}

/*
 * Close the connection to the database.
 */
static void mysqldb_destroy(const char *zone, void *driverdata, void **dbdata)
{
    struct dbinfo *dbi = *dbdata;

    UNUSED(zone);
    UNUSED(driverdata);

    mysql_close(&dbi->conn);
    if (dbi->database != NULL)
        isc_mem_free(ns_g_mctx, dbi->database);
    if (dbi->table != NULL)
        isc_mem_free(ns_g_mctx, dbi->table);
    if (dbi->host != NULL)
        isc_mem_free(ns_g_mctx, dbi->host);
    if (dbi->user != NULL)
        isc_mem_free(ns_g_mctx, dbi->user);
    if (dbi->passwd != NULL)
        isc_mem_free(ns_g_mctx, dbi->passwd);
    if (dbi->domain_id != NULL)
        isc_mem_free(ns_g_mctx, dbi->domain_id);
    if (dbi->tenant_id != NULL)
        isc_mem_free(ns_g_mctx, dbi->tenant_id);
    if (dbi->database != NULL)
        isc_mem_free(ns_g_mctx, dbi->database);
    isc_mem_put(ns_g_mctx, dbi, sizeof(struct dbinfo));
}

/*
 * Since the SQL database corresponds to a zone, the authority data should
 * be returned by the lookup() function.  Therefore the authority() function
 * is NULL.
 */
static dns_sdbmethods_t mysqldb_methods = {
    mysqldb_lookup,
    NULL, /* authority */
    mysqldb_allnodes,
    mysqldb_create,
    mysqldb_destroy
};

/*
 * Wrapper around dns_sdb_register().
 */
isc_result_t mysqldb_init(void)
{
    unsigned int flags;
    flags = 0;
    return (dns_sdb_register("mysqldb", &mysqldb_methods, NULL, flags,
            ns_g_mctx, &mysqldb));
}

/*
 * Wrapper around dns_sdb_unregister().
 */
void mysqldb_clear(void)
{
    if (mysqldb != NULL)
        dns_sdb_unregister(&mysqldb);
}
