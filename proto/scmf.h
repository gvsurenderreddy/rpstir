/*
  $Id$
*/

#ifndef _SCMF_H_
#define _SCMF_H_

#include <sql.h>
#include <sqlext.h>

typedef struct _scmstat		/* connection statistics */
{
  char     *errmsg;		/* error messages */
  char     *tabname;            /* name of table having error: not allocated */
  int       emlen;		/* alloc'd length of errmsg */
  int       rows;		/* rows changed */
} scmstat;

typedef struct _scmcon		/* connection info */
{
  SQLHENV   henv;		/* environment handle */
  SQLHDBC   hdbc;		/* database handle */
  SQLHSTMT  hstmt;		/* statement handle */
  int       connected;		/* are we connected? */
  scmstat   mystat;		/* statistics and errors */
} scmcon;

typedef struct _scmkv		/* used for a single column of an insert */
{
  char     *column;		/* column name */
  char     *value;		/* value for that column */
} scmkv;

typedef struct _scmkva		/* used for an insert */
{
  scmkv    *vec;		/* array of column/value pairs */
  int       ntot;		/* total length of "vec" */
  int       nused;		/* number of elements of "vec" in use */
  int       vald;		/* struct already validated? */
} scmkva;

typedef struct _scmsrch		/* used for a single column of a search */
{
  int       colno;		/* column number in result, typically idx+1 */
  int       sqltype;		/* SQL C type, e.q. SQL_C_ULONG */
  char     *colname;		/* name of column */
  void     *valptr;		/* where the value goes */
  unsigned  valsize;		/* expected value size */
  int       avalsize;		/* actual value size, really an SQLINTEGER */
} scmsrch;

typedef struct _scmsrcha	/* used for a search (select) */
{
  scmsrch  *vec;		/* array of column info */
  char     *sname;		/* unique name for this search (can be NULL) */
  int       ntot;		/* total length of "vec" */
  int       nused;		/* number of elements in vec */
  int       vald;		/* struct already validated? */
  void     *context;		/* context to be passed from callback */
} scmsrcha;

// callback function signature for a count of search results

typedef int (*sqlcountfunc)(scmcon *conp, scmsrcha *s, int cnt);

// callback function signature for a single search result

typedef int (*sqlvaluefunc)(scmcon *conp, scmsrcha *s, int idx);

// bitfields for how to do a search

#define SCM_SRCH_DOCOUNT         0x1   /* call count func */
#define SCM_SRCH_DOVALUE_ANN     0x2   /* call val func if all vals non-NULL */
#define SCM_SRCH_DOVALUE_SNN     0x4   /* call val func if some vals non-NULL */
#define SCM_SRCH_DOVALUE_ALWAYS  0x8   /* always call value func */
#define SCM_SRCH_DOVALUE         0xE   /* call value func */
#define SCM_SRCH_BREAK_CERR      0x10  /* break from loop if count func err */
#define SCM_SRCH_BREAK_VERR      0x20  /* break from loop if value func err */

#ifndef SQLOK
#define SQLOK(s) (s == SQL_SUCCESS || s == SQL_SUCCESS_WITH_INFO)
#endif

extern scmcon   *connectscm(char *dsnp, char *errmsg, int emlen);
extern scmsrcha *newsrchscm(char *name, int leen, int cleen);
extern void  disconnectscm(scmcon *conp);
extern char *geterrorscm(scmcon *conp);
extern char *gettablescm(scmcon *conp);
extern int   getrowsscm(scmcon *conp);
extern int   statementscm(scmcon *conp, char *stm);
extern int   createdbscm(scmcon *conp, char *dbname, char *dbuser);
extern int   deletedbscm(scmcon *conp, char *dbname);
extern int   createalltablesscm(scmcon *conp, scm *scmp);
extern int   insertscm(scmcon *conp, scmtab *tabp, scmkva *arr);
extern int   getmaxidscm(scm *scmp, scmcon *conp, scmtab *mtab,
			 char *what, unsigned int *ival);
extern int   setmaxidscm(scm *scmp, scmcon *conp, scmtab *mtab,
			   char *what, unsigned int ival);
extern int   getuintscm(scmcon *conp, unsigned int *ival);
extern int   searchscm(scmcon *conp, scmtab *tabp, scmsrcha *srch,
		       sqlcountfunc cnter, sqlvaluefunc valer,
		       int what);
extern int   addcolsrchscm(scmsrcha *srch, char *colname, int sqltype,
			   unsigned valsize);
extern void  freesrchscm(scmsrcha *srch);

/*
  Error codes
*/

#define ERR_SCM_NOERR         0
#define ERR_SCM_COFILE       -1  	/* cannot open file */
#define ERR_SCM_NOMEM        -2	        /* out of memory */
#define ERR_SCM_INVALARG     -3	        /* invalid argument */
#define ERR_SCM_SQL          -4         /* SQL error */
#define ERR_SCM_INVALCOL     -5	        /* invalid column */
#define ERR_SCM_NULLCOL      -6         /* null column */
#define ERR_SCM_NOSUCHTAB    -7         /* no such table */
#define ERR_SCM_NODATA       -8         /* no matching data in table */
#define ERR_SCM_NULLVALP     -9         /* null value pointer */
#define ERR_SCM_INVALSZ     -10         /* invalid size */

#endif
