/**
 * @file
 *
 * @brief
 *     Functions and flags shared by query and server code
 */

#include "querySupport.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mysql.h>
#include <arpa/inet.h>

#include "config/config.h"

#include "globals.h"
#include "scm.h"
#include "scmf.h"
#include "sqhl.h"
#include "cms/roa_utils.h"
#include "err.h"
#include "myssl.h"
#include "util/logging.h"
#include "util/stringutils.h"

void addQueryFlagTests(
    char *whereStr,
    int needAnd)
{
    // NOTE: This must be kept in sync with flag_tests_default.

    addFlagTest(whereStr, SCM_FLAG_VALIDATED, 1, needAnd);
    addFlagTest(whereStr, SCM_FLAG_NOCHAIN, 0, 1);
    if (!CONFIG_RPKI_ALLOW_STALE_CRL_get())
        addFlagTest(whereStr, SCM_FLAG_STALECRL, 0, 1);
    if (!CONFIG_RPKI_ALLOW_STALE_MANIFEST_get())
        addFlagTest(whereStr, SCM_FLAG_STALEMAN, 0, 1);
    if (!CONFIG_RPKI_ALLOW_NO_MANIFEST_get())
        addFlagTest(whereStr, SCM_FLAG_ONMAN, 1, 1);
    if (!CONFIG_RPKI_ALLOW_NOT_YET_get())
        addFlagTest(whereStr, SCM_FLAG_NOTYET, 0, 1);
}


/*
 * all these static variables are used for efficiency, so that
 * there is no need to initialize them with each call to checkValidity
 */
static scmtab *roaPrefixTable = NULL;
static scmtab *validTable = NULL;
static scmsrcha *validSrch = NULL;
char *validWhereStr;
static char *whereInsertPtr;
static int parentsFound;
static char *nextSKI,
   *nextSubject;

/**
 * @brief
 *     callback to indicate that a parent was found
 */
static sqlvaluefunc registerParent;
err_code
registerParent(
    scmcon *conp,
    scmsrcha *s,
    ssize_t numLine)
{
    UNREFERENCED_PARAMETER(conp);
    UNREFERENCED_PARAMETER(s);
    UNREFERENCED_PARAMETER(numLine);

    /* FIXME: nextSKI and nextSubject already point to
     * s->vec[0].valptr and s->vec[1].valptr, so they are not updated
     * here.  But when a column is NULL, avalsize = SQL_NULL_DATA
     * (-1), and we should NOT be using the corresponding valptr.
     * checkValidity() currently depends on valptr being zeroed out.
     * This is Bad(TM).  Fix this on a rewrite of checkValidity. */

    /* Count parent. */
    parentsFound++;
    return 0;
}

/**
 * @brief set up main part of query
 *
 * Global variables are initialized only once, instead of once per
 * object.
 *
 * @param[in] scmp
 *     Database schema pointer.
 */
static void
initSearch(
    scm *scmp)
{
    if (validTable)
        return;

    validTable = findtablescm(scmp, "certificate");
    validSrch = newsrchscm(NULL, 3, 0, 1);
    QueryField *field = findField("aki");
    /** @bug ignores error code without explanation */
    addcolsrchscm(validSrch, "aki", field->sqlType, field->maxSize);
    /** @bug ignores error code without explanation */
    char *now = LocalTimeToDBTime(NULL);
    field = findField("issuer");
    /** @bug ignores error code without explanation */
    addcolsrchscm(validSrch, "issuer", field->sqlType, field->maxSize);
    validWhereStr = validSrch->wherestr;
    validWhereStr[0] = 0;
    xsnprintf(validWhereStr, WHERESTR_SIZE, "valto>\"%s\"", now);
    free(now);
    addFlagTest(validWhereStr, SCM_FLAG_VALIDATED, 1, 1);
    addFlagTest(validWhereStr, SCM_FLAG_NOCHAIN, 0, 1);
    if (!CONFIG_RPKI_ALLOW_STALE_CRL_get())
        addFlagTest(validWhereStr, SCM_FLAG_STALECRL, 0, 1);
    if (!CONFIG_RPKI_ALLOW_STALE_MANIFEST_get())
        addFlagTest(validWhereStr, SCM_FLAG_STALEMAN, 0, 1);
    if (!CONFIG_RPKI_ALLOW_NOT_YET_get())
        addFlagTest(validWhereStr, SCM_FLAG_NOTYET, 0, 1);
    if (!CONFIG_RPKI_ALLOW_NO_MANIFEST_get())
    {
        int len = strlen(validWhereStr);
        xsnprintf(&validWhereStr[len], WHERESTR_SIZE - len,
                  " and ("
                  "((flags%%%d)>=%d)"
                  " or ((flags%%%d)<%d)"
                  " or ((flags%%%d)>=%d)"
                  ")",
                  2 * SCM_FLAG_ONMAN, SCM_FLAG_ONMAN, 2 * SCM_FLAG_CA,
                  SCM_FLAG_CA, 2 * SCM_FLAG_TRUSTED, SCM_FLAG_TRUSTED);
    }
    whereInsertPtr = &validWhereStr[strlen(validWhereStr)];
    nextSKI = (char *)validSrch->vec[0].valptr;
    nextSubject = (char *)validSrch->vec[1].valptr;
}

int checkValidity(
    char *ski,
    unsigned int localID,
    scm *scmp,
    scmcon *connect)
{
    initSearch(scmp);

    /* FIXME: This code assumes that is suffices to trace a single
     * parent until one arrives at a trust anchor.  This will not
     * always be the case, so key rollover or malicious activity might
     * break the query client.  In addition, the right behavior is to
     * trace up to any TRUSTED cert, which is not necessarily
     * equivalent to any SELF-SIGNED cert.  Fix this on a future
     * rewrite of checkValidity().  */

    // now do the part specific to this cert
    int firstTime = 1;
    char prevSKI[128];
    // keep going until trust anchor, where either AKI = SKI or no AKI
    while (firstTime ||
           !(strcmp(nextSKI, prevSKI) == 0 || strlen(nextSKI) == 0))
    {
        if (firstTime)
        {
            firstTime = 0;
            if (ski)
            {
                xsnprintf(whereInsertPtr, WHERESTR_SIZE - strlen(validWhereStr),
                          " and ski=\"%s\"", ski);
                strncpy(prevSKI, ski, 128);
            }
            else
            {
                xsnprintf(whereInsertPtr, WHERESTR_SIZE - strlen(validWhereStr),
                          " and local_id=\"%d\"", localID);
                prevSKI[0] = 0;
            }
        }
        else
        {
            char escaped_subject[2 * strlen(nextSubject) + 1];
            mysql_escape_string(escaped_subject, nextSubject,
                                strlen(nextSubject));
            xsnprintf(whereInsertPtr, WHERESTR_SIZE - strlen(validWhereStr),
                      " and ski=\"%s\" and subject=\"%s\"", nextSKI,
                      escaped_subject);
            strncpy(prevSKI, nextSKI, 128);
        }
        parentsFound = 0;
        /** @bug ignores error code without explanation */
        searchscm(connect, validTable, validSrch, NULL,
                  &registerParent, SCM_SRCH_DOVALUE_ALWAYS, NULL);
        if (parentsFound > 1)
        {
            LOG(LOG_WARNING, "multiple parents (%d) found; results suspect",
                parentsFound);
        }
        else if (parentsFound == 0)
        {
            // no parent cert
            return 0;
        }
    }
    return 1;
}


/**
 * @brief
 *     combines dirname and filename into a pathname
 */
static displayfunc pathnameDisplay;
int
pathnameDisplay(
    scm *scmp,
    scmcon *connection,
    scmsrcha *s,
    int idx1,
    char *returnStr)
{
    (void)scmp;
    (void)connection;
    xsnprintf(returnStr, MAX_RESULT_SZ, "%s/%s",
              (char *)s->vec[idx1].valptr, (char *)s->vec[idx1 + 1].valptr);
    return 2;
}

/**
 * @brief
 *     create space-separated string of serial numbers
 */
static displayfunc displaySNList;
int
displaySNList(
    scm *scmp,
    scmcon *connection,
    scmsrcha *s,
    int idx1,
    char *returnStr)
{
    (void)scmp;
    (void)connection;

    uint8_t *snlist;
    unsigned int i,
        snlen;
    char *hexs;
    char nomem[] = "out-of-memory";

    snlen = *((unsigned int *)(s->vec[idx1].valptr));
    snlist = (uint8_t *)s->vec[idx1 + 1].valptr;
    returnStr[0] = 0;
    for (i = 0; i < snlen; i++)
    {
        hexs = hexify(SER_NUM_MAX_SZ, &snlist[SER_NUM_MAX_SZ * i], HEXIFY_X);
        if (hexs == NULL)
        {
            // XXX: there should be a better way to signal an error
            hexs = nomem;
        }
        xsnprintf(&returnStr[strlen(returnStr)],
                  MAX_RESULT_SZ - strlen(returnStr), "%s%s",
                  (i == 0) ? "" : " ", hexs);
        if (hexs == nomem)
        {
            break;
        }
        free(hexs);
    }
    return 2;
}

/**
 * @brief
 *     callback state for the display_ip_addrs_valuefunc() callback
 *     function
 */
struct display_ip_addrs_context
{
    char const *separator;

    char *result;
    size_t result_len;
    size_t result_idx;
};

/**
 * @brief
 *     searchscm() callback for display_ip_addrs(), called for each
 *     prefix in a ROA
 */
static sqlvaluefunc display_ip_addrs_valuefunc;
err_code
display_ip_addrs_valuefunc(
    scmcon *conp,
    scmsrcha *s,
    ssize_t idx)
{
    (void)conp;
    (void)idx;

    struct display_ip_addrs_context *context = s->context;

    unsigned char const *prefix = s->vec[0].valptr;
    int_fast64_t const prefix_family_length = s->vec[0].avalsize;
    unsigned long const prefix_length =
        *(unsigned long const *)s->vec[1].valptr;
    unsigned long const prefix_max_length =
        *(unsigned long const *)s->vec[2].valptr;

    int af;
    switch (prefix_family_length)
    {
        case 4:
            af = AF_INET;
            break;

        case 16:
            af = AF_INET6;
            break;

        default:
            LOG(LOG_ERR, "invalid prefix_family_length %" PRIuFAST64,
                prefix_family_length);
            return ERR_SCM_INTERNAL;
    }

    char prefix_str[INET6_ADDRSTRLEN];
    if (inet_ntop(af, prefix, prefix_str, sizeof(prefix_str)) == NULL)
    {
        LOG(LOG_ERR, "error converting prefix to a string");
        return ERR_SCM_INTERNAL;
    }

    int snprintf_ret = snprintf(
        context->result + context->result_idx,
        context->result_len - context->result_idx,
        "%s/%lu(%lu)%s",
        prefix_str,
        prefix_length,
        prefix_max_length,
        context->separator);
    if (snprintf_ret < 0)
    {
        return ERR_SCM_INTERNAL;
    }
    else if ((size_t)snprintf_ret >=
        context->result_len - context->result_idx)
    {
        return ERR_SCM_TRUNCATED;
    }
    else
    {
        context->result_idx += snprintf_ret;
    }

    return 0;
}

/**
 * @brief
 *     displayfunc to convert the list of prefixes in a ROA into a
 *     string, @p returnStr
 */
static displayfunc display_ip_addrs;
int
display_ip_addrs(
    scm *scmp,
    scmcon *connection,
    scmsrcha *s,
    int idx1,
    char *returnStr)
{
    static char const none_str[] = "(none)";
    static char const truncated_str[] = "...";
    static char const separator[] = ", ";
    static size_t const separator_len = sizeof(separator) - 1;

    if (roaPrefixTable == NULL)
    {
        roaPrefixTable = findtablescm(scmp, "ROA_PREFIX");
    }

    err_code sta;

    unsigned long roa_local_id =
        *((unsigned long *)(s->vec[idx1].valptr));
    char roa_local_id_str[24];
    xsnprintf(roa_local_id_str, sizeof(roa_local_id_str), "%lu",
        roa_local_id);

    struct display_ip_addrs_context context;
    context.separator = separator;
    context.result = returnStr;
    context.result[0] = '\0';
    context.result_len = MAX_RESULT_SZ - sizeof(truncated_str);
    context.result_idx = 0;

    unsigned char prefix[16];
    unsigned long prefix_length;
    unsigned long prefix_max_length;

    scmsrch select[3];
    scmkv where_cols[1];
    scmkva where;
    char * order;
    scmsrcha srch;

    select[0].colno = 1;
    select[0].sqltype = SQL_C_BINARY;
    select[0].colname = "prefix";
    select[0].valptr = prefix;
    select[0].valsize = sizeof(prefix);
    select[0].avalsize = 0;
    select[1].colno = 2;
    select[1].sqltype = SQL_C_ULONG;
    select[1].colname = "prefix_length";
    select[1].valptr = &prefix_length;
    select[1].valsize = sizeof(prefix_length);
    select[1].avalsize = 0;
    select[2].colno = 3;
    select[2].sqltype = SQL_C_ULONG;
    select[2].colname = "prefix_max_length";
    select[2].valptr = &prefix_max_length;
    select[2].valsize = sizeof(prefix_max_length);
    select[2].avalsize = 0;

    where_cols[0].column = "roa_local_id";
    where_cols[0].value = roa_local_id_str;

    where.vec = where_cols;
    where.ntot = sizeof(where_cols)/sizeof(where_cols[0]);
    where.nused = where.ntot;
    where.vald = 0;

    order =
        "length(prefix) asc, "
        "prefix asc, "
        "prefix_length asc, "
        "prefix_max_length asc";

    srch.vec = select;
    srch.sname = NULL;
    srch.ntot = sizeof(select)/sizeof(select[0]);
    srch.nused = srch.ntot;
    srch.vald = 0;
    srch.where = &where;
    srch.wherestr = NULL;
    srch.context = &context;

    sta = searchscm(
        connection,
        roaPrefixTable,
        &srch,
        NULL,
        &display_ip_addrs_valuefunc,
        SCM_SRCH_DOVALUE_ANN | SCM_SRCH_BREAK_VERR,
        order);
    if (sta == ERR_SCM_TRUNCATED)
    {
        xsnprintf(
            context.result + context.result_idx,
            MAX_RESULT_SZ - context.result_idx,
            "%s",
            truncated_str);
    }
    else if (sta < 0)
    {
        // XXX: there should be a better way to signal an error
        xsnprintf(
            context.result,
            MAX_RESULT_SZ,
            "error: %s (%d)",
            err2string(sta),
            sta);
    }
    else if (context.result_idx >= separator_len)
    {
        // Remove the final separator
        context.result[context.result_idx - separator_len] = '\0';
    }
    else if (context.result[0] == '\0')
    {
        // Indicate that there were no results
        xsnprintf(context.result, MAX_RESULT_SZ, "%s", none_str);
    }

    return 1;
}

/**
 * @brief
 *     helper function for displayFlags
 */
static void addFlagIfSet(
    char *returnStr,
    unsigned int flags,
    unsigned int flag,
    char *str)
{
    if (flags & flag)
    {
        xsnprintf(&returnStr[strlen(returnStr)],
                  MAX_RESULT_SZ - strlen(returnStr), "%s%s",
                  (returnStr[0] == 0) ? "" : " | ", str);
    }
}

static void addFlagIfUnset(
    char *returnStr,
    unsigned int flags,
    unsigned int flag,
    char *str)
{
    if (!(flags & flag))
    {
        xsnprintf(&returnStr[strlen(returnStr)],
                  MAX_RESULT_SZ - strlen(returnStr), "%s%s",
                  (returnStr[0] == 0) ? "" : " | ", str);
    }
}

static int isManifest = 0;

void setIsManifest(
    int val)
{
    isManifest = val;
}

/**
 * @brief
 *     create list of all flags set to true
 */
static displayfunc displayFlags;
int
displayFlags(
    scm *scmp,
    scmcon *connection,
    scmsrcha *s,
    int idx1,
    char *returnStr)
{
    (void)scmp;
    (void)connection;
    unsigned int flags = *((unsigned int *)(s->vec[idx1].valptr));
    returnStr[0] = 0;
    addFlagIfSet(returnStr, flags, SCM_FLAG_CA, "CA");
    addFlagIfSet(returnStr, flags, SCM_FLAG_TRUSTED, "TRUSTED");
    addFlagIfSet(returnStr, flags, SCM_FLAG_VALIDATED, "VALIDATED");
    if ((flags & SCM_FLAG_VALIDATED))
    {
        if ((flags & SCM_FLAG_NOCHAIN))
            addFlagIfSet(returnStr, flags, SCM_FLAG_NOCHAIN, "NOCHAIN");
        else
            addFlagIfUnset(returnStr, flags, SCM_FLAG_NOCHAIN, "CHAINED");
    }
    addFlagIfSet(returnStr, flags, SCM_FLAG_NOTYET, "NOTYET");
    addFlagIfSet(returnStr, flags, SCM_FLAG_STALECRL, "STALECRL");
    addFlagIfSet(returnStr, flags, SCM_FLAG_STALEMAN, "STALEMAN");
    if (!isManifest)
    {
        addFlagIfSet(returnStr, flags, SCM_FLAG_ONMAN, "ONMAN");
    }
    return 1;
}

/**
 * @brief
 *     the set of all query fields
 */
static QueryField fields[] = {
    {
        "filename",
        "the filename where the data is stored in the repository",
        Q_FOR_ROA | Q_FOR_CRL | Q_FOR_CERT | Q_FOR_MAN | Q_FOR_GBR,
        SQL_C_CHAR, FNAMESIZE,
        NULL, NULL,
        "Filename", NULL,
    },
    {
        "pathname",
        "full pathname (directory plus filename) where the data is stored",
        Q_JUST_DISPLAY | Q_FOR_ROA | Q_FOR_CERT | Q_FOR_CRL | Q_FOR_MAN |
        Q_FOR_GBR | Q_REQ_JOIN,
        -1, 0,
        "dirname", "filename",
        "Pathname", &pathnameDisplay,
    },
    {
        "dirname",
        "the directory in the repository where the data is stored",
        Q_FOR_ROA | Q_FOR_CRL | Q_FOR_CERT | Q_FOR_MAN | Q_FOR_GBR | Q_REQ_JOIN,
        SQL_C_CHAR, DNAMESIZE,
        NULL, NULL,
        "Directory", NULL,
    },
    {
        "ski",
        "subject key identifier",
        Q_FOR_ROA | Q_FOR_CERT | Q_FOR_MAN | Q_FOR_GBR,
        SQL_C_CHAR, SKISIZE,
        NULL, NULL,
        "SKI", NULL,
    },
    {
        "aki",
        "authority key identifier",
        Q_FOR_CRL | Q_FOR_CERT,
        SQL_C_CHAR, SKISIZE,
        NULL, NULL,
        "AKI", NULL,
    },
    {
        "sia",
        "Subject Information Access",
        Q_FOR_CERT,
        SQL_C_CHAR, SIASIZE,
        NULL, NULL,
        "SIA", NULL,
    },
    {
        "aia",
        "Authority Information Access",
        Q_FOR_CERT,
        SQL_C_CHAR, SIASIZE,
        NULL, NULL,
        "AIA", NULL,
    },
    {
        "crldp",
        "CRL Distribution Points",
        Q_FOR_CERT,
        SQL_C_CHAR, SIASIZE,
        NULL, NULL,
        "CRLDP", NULL,
    },
    {
        "local_id",
        NULL,
        Q_JUST_DISPLAY | Q_FOR_ROA,
        SQL_C_ULONG, sizeof(unsigned long),
        NULL, NULL,
        NULL, NULL,
    },
    {
        "ip_addrs",
        "the set of IP addresses assigned by the ROA",
        Q_JUST_DISPLAY | Q_FOR_ROA,
        -1, 0,
        "local_id",
        NULL,
        "IP Addresses",
        &display_ip_addrs,
    },
    {
        "asn",
        "autonomous system number",
        Q_FOR_ROA,
        SQL_C_ULONG, 8,
        NULL, NULL,
        "AS#", NULL,
    },
    {
        "issuer",
        "system that issued the cert/crl",
        Q_FOR_CERT | Q_FOR_CRL,
        SQL_C_CHAR, SUBJSIZE,
        NULL, NULL,
        "Issuer", NULL,
    },
    {
        "valfrom",
        "date/time from which the cert is valid",
        Q_FOR_CERT,
        SQL_C_CHAR, 32,
        NULL, NULL,
        "Valid From", NULL,
    },
    {
        "valto",
        "date/time to which the cert is valid",
        Q_FOR_CERT,
        SQL_C_CHAR, 32,
        NULL, NULL,
        "Valid To", NULL,
    },
    {
        "last_upd",
        "last update time of the object",
        Q_FOR_CRL,
        SQL_C_CHAR, 32,
        NULL, NULL,
        "Last Update", NULL,
    },
    {
        "this_upd",
        "last update time of the object",
        Q_FOR_MAN,
        SQL_C_CHAR, 32,
        NULL, NULL,
        "This Update", NULL,
    },
    {
        "next_upd",
        "next update time of the object",
        Q_FOR_CRL | Q_FOR_MAN,
        SQL_C_CHAR, 32,
        NULL, NULL,
        "Next Update", NULL,
    },
    {
        "crlno",
        "CRL number",
        Q_JUST_DISPLAY | Q_FOR_CRL,
        SQL_C_BINARY, 20,
        NULL, NULL,
        "CRL#", NULL,
    },
    {
        "sn",
        "serial number",
        Q_JUST_DISPLAY | Q_FOR_CERT,
        SQL_C_BINARY, 20,
        NULL, NULL,
        "Serial#", NULL,
    },
    {
        "snlen",
        "number of serial numbers in crl",
        Q_FOR_CRL,
        SQL_C_ULONG, 8,
        NULL, NULL,
        "SNLength", NULL,
    },
    {
        "snlist",
        NULL,
        Q_JUST_DISPLAY | Q_FOR_CRL,
        SQL_C_BINARY, 16000000,
        NULL, NULL,
        NULL, NULL,
    },
    {
        "files",
        "All the filenames in the manifest",
        Q_JUST_DISPLAY | Q_FOR_MAN,
        SQL_C_BINARY, 160000,
        NULL, NULL,
        "FilesInMan", NULL,
    },
    {
        "serial_nums",
        "list of serials numbers",
        Q_JUST_DISPLAY | Q_FOR_CRL,
        -1, 0,
        "snlen", "snlist",
        "Serial#s", &displaySNList,
    },
    {
        "flags",
        "which flags are set in the database",
        Q_JUST_DISPLAY | Q_FOR_CERT | Q_FOR_CRL
        | Q_FOR_ROA | Q_FOR_MAN | Q_FOR_GBR,
        SQL_C_ULONG, 8,
        NULL, NULL,
        "Flags Set", &displayFlags,
    }
};

QueryField *findField(
    char *name)
{
    int i;
    int size = sizeof(fields) / sizeof(fields[0]);
    for (i = 0; i < size; i++)
    {
        if (strcasecmp(name, fields[i].name) == 0)
            return &fields[i];
    }
    return NULL;
}

QueryField *getFields(
    )
{
    return fields;
}

int getNumFields(
    )
{
    return countof(fields);
}
