/* dbops.c
 * Database operations.
 * Copyright (c) Karl Dahlke, 2007
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"
#include "dbapi.h"

const char *sql_debuglog = "ebsql.log";	/* log of debug prints */
const char *sql_database;	/* name of current database */

char *
lineFormat(const char *line, ...)
{
    char *s;
    va_list p;
    va_start(p, line);
    s = lineFormatStack(line, 0, &p);
    va_end(p);
    return s;
}				/* lineFormat */

#define LFBUFSIZE 4000
static char lfbuf[LFBUFSIZE];	/* line formatting buffer */
static const char selfref[] =
   "@lineFormat attempts to expand within its own static buffer";
static const char lfoverflow[] = "@lineFormat(), line is too long, limit %d";

char *
lineFormatStack(const char *line,	/* the sprintf-like formatting string */
   LF * argv,			/* pointer to array of values */
   va_list * parmv_p)
{				/* pointers to parameters on the stack */
    va_list parmv = 0;
    short i, len, maxlen, len_given, flags;
    long n;
    double dn;			/* double number */
    char *q, pdir, inquote;
    const char *t, *perc;
    char fmt[12];

    if(parmv_p)
	parmv = *parmv_p;
    if(parmv && argv || !parmv && !argv)
	errorPrint
	   ("@exactly one of the last two arguments to lineFormatStack should be null");

    if(line == lfbuf) {
	if(strchr(line, '%'))
	    errorPrint(selfref);
	return (char *)line;
    }

    lfbuf[0] = 0;
    q = lfbuf;
    t = line;

    while(*t) {			/* more text to format */
/* copy up to the next % */
	if(*t != '%' || t[1] == '%' && ++t) {
	    if(q - lfbuf >= LFBUFSIZE - 1)
		errorPrint(lfoverflow, LFBUFSIZE);
	    *q++ = *t++;
	    continue;
	}

/* % found */
	perc = t++;
	inquote = 0;
	len = 0;
	len_given = 0;

	if(*t == '-')
	    ++t;
	for(; isdigit(*t); ++t) {
	    len_given = 1;
	    len = 10 * len + *t - '0';
	}
	while(*t == '.' || isdigit(*t))
	    ++t;
	pdir = *t++;
	if(isupper(pdir)) {
	    pdir = tolower(pdir);
	    inquote = '"';
	}
	if(t - perc >= sizeof (fmt))
	    errorPrint("2percent directive in lineFormat too long");
	strncpy(fmt, perc, t - perc);
	fmt[t - perc] = 0;
	maxlen = len;
	if(maxlen < 11)
	    maxlen = 11;

/* get the next vararg */
	if(pdir == 'f') {
	    if(parmv)
		dn = va_arg(parmv, double);
	    else
		dn = argv->f;
	} else {
	    if(parmv)
		n = va_arg(parmv, int);
	    else
		n = argv->l;
	}
	if(argv)
	    ++argv;

	if(pdir == 's' && n) {
	    i = strlen((char *)n);
	    if(i > maxlen)
		maxlen = i;
	    if(inquote && strchr((char *)n, inquote)) {
		inquote = '\'';
		if(strchr((char *)n, inquote))
		    errorPrint("2lineFormat() cannot put quotes around %s", n);
	    }
	}
	if(inquote)
	    maxlen += 2;
	if(q + maxlen >= lfbuf + LFBUFSIZE)
	    errorPrint(lfoverflow, LFBUFSIZE);

/* check for null parameter */
	if(pdir == 'c' && !n ||
	   pdir == 's' && isnullstring((char *)n) ||
	   pdir == 'f' && dn == nullfloat ||
	   !strchr("scf", pdir) && isnull(n)) {
	    if(!len_given) {
		char *q1;
/* turn = %d to is null */
		for(q1 = q - 1; q1 >= lfbuf && *q1 == ' '; --q1) ;
		if(q1 >= lfbuf && *q1 == '=') {
		    if(q1 > lfbuf && q1[-1] == '!') {
			strcpy(q1 - 1, "IS NOT ");
			q = q1 + 6;
		    } else {
			strcpy(q1, "IS ");
			q = q1 + 3;
		    }
		}
		strcpy(q, "NULL");
		q += 4;
		continue;
	    }			/* null with no length specified */
	    pdir = 's';
	    n = (int)"";
	}
	/* parameter is null */
	if(inquote)
	    *q++ = inquote;
	fmt[t - perc - 1] = pdir;
	switch (pdir) {
	case 'i':
	    flags = DTDELIMIT;
	    if(len) {
		if(len >= 11)
		    flags |= DTAMPM;
		if(len < 8)
		    flags = DTDELIMIT | DTCRUNCH;
		if(len < 5)
		    flags = DTCRUNCH;
	    }
	    strcpy(q, timeString(n, flags));
	    break;
	case 'a':
	    flags = DTDELIMIT;
	    if(len) {
		if(len < 10)
		    flags = DTCRUNCH | DTDELIMIT;
		if(len < 8)
		    flags = DTCRUNCH;
		if(len == 5)
		    flags = DTCRUNCH | DTDELIMIT;
	    }
	    strcpy(q, dateString(n, flags));
	    if(len == 4 || len == 5)
		q[len] = 0;
	    break;
	case 'm':
	    strcpy(q, moneyString(n));
	    break;
	case 'f':
	    sprintf(q, fmt, dn);
	    break;
	case 's':
	    if(n == (int)lfbuf)
		errorPrint(selfref);
/* extra code to prevent %09s from printing out all zeros
when the argument is null (empty string) */
	    if(!*(char *)n && fmt[1] == '0')
		strcpy(fmt + 1, fmt + 2);
/* fall through */
	default:
	    sprintf(q, fmt, n);
	}			/* switch */
	q += strlen(q);
	if(inquote)
	    *q++ = inquote;
    }				/* loop printing pieces of the string */

    *q = 0;			/* null terminate */

/* we relie on the calling function to invoke va_end(), since the arg list
is not always the ... varargs of a function, though it usually is.
See lineFormat() above for a typical example.
Note that the calling function may wish to process additional arguments
before calling va_end. */

    if(parmv)
	*parmv_p = parmv;
    return lfbuf;
}				/* lineFormatStack */

/* given a datatype, return the character that, when appended to %,
causes lineFormat() to print the data element properly. */
static char
sprintfChar(char datatype)
{
    char c;
    switch (datatype) {
    case 'S':
	c = 's';
	break;
    case 'C':
	c = 'c';
	break;
    case 'M':
    case 'N':
	c = 'd';
	break;
    case 'D':
	c = 'a';
	break;
    case 'I':
	c = 'i';
	break;
    case 'F':
	c = 'f';
	break;
    case 'B':
    case 'T':
	c = 'd';
	break;
    default:
	c = 0;
    }				/* switch */
    return c;
}				/* sprintfChar */

/*********************************************************************
Using the values just fetched or selected, build a line in unload format.
All fields are expanded into ascii, with pipes between them.
Conversely, given a line of pipe separated fields,
put them back into binary, ready for retsCopy().
*********************************************************************/

char *
sql_mkunld(char delim)
{
    char fmt[NUMRETS * 4 + 1];
    int i;
    char pftype;

    for(i = 0; i < rv_numRets; ++i) {
	pftype = sprintfChar(rv_type[i]);
	if(!pftype)
	    errorPrint("2sql_mkunld cannot convert datatype %c", rv_type[i]);
	sprintf(fmt + 4 * i, "%%0%c%c", pftype, delim);
    }				/* loop over returns */

    return lineFormatStack(fmt, rv_data, 0);
}				/* sql_mkunld */

/* like the above, but we build a comma-separated list with quotes,
ready for SQL insert or update.
You might be tempted to call this routine first, obtaining a string,
and then call lineFormat("insert into foo values(%s)",
but don't do that!
The returned string is built by lineFormat and is already in the buffer.
You instead need to make a copy of the string and then call lineFormat. */
char *
sql_mkinsupd()
{
    char fmt[NUMRETS * 3 + 1];
    int i;
    char pftype;

    for(i = 0; i < rv_numRets; ++i) {
	pftype = sprintfChar(rv_type[i]);
	if(!pftype)
	    errorPrint("2sql_mkinsupd cannot convert datatype %c", rv_type[i]);
	if(pftype != 'd' && pftype != 'f')
	    pftype = toupper(pftype);
	sprintf(fmt + 3 * i, "%%%c,", pftype);
    }				/* loop over returns */
    fmt[3 * i - 1] = 0;

    return lineFormatStack(fmt, rv_data, 0);
}				/* sql_mkinsupd */


/*********************************************************************
Date time functions.
*********************************************************************/

static char ndays[] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

bool
isLeapYear(int year)
{
    if(year % 4)
	return false;
    if(year % 100)
	return true;
    if(year % 400)
	return false;
    return true;
}				/* isLeapYear */

/* convert year, month, and day into a date. */
/* return -1 = bad year, -2 = bad month, -3 = bad day */
date
dateEncode(int year, int month, int day)
{
    short i;
    long d;
    if((year | month | day) == 0)
	return nullint;
    if(year < 1640 || year > 3000)
	return -1;
    if(month <= 0 || month > 12)
	return -2;
    if(day <= 0 || day > ndays[month])
	return -3;
    if(day == 29 && month == 2 && !isLeapYear(year))
	return -3;
    --year;
    d = year * 365L + year / 4 - year / 100 + year / 400;
    for(i = 1; i < month; ++i)
	d += ndays[i];
    ++year;
    if(month > 2 && !isLeapYear(year))
	--d;
    d += (day - 1);
    d -= 598632;
    return d;
}				/* dateEncode */

/* convert a date back into year, month, and day */
/* the inverse of the above */
void
dateDecode(date d, int *yp, int *mp, int *dp)
{
    int year, month, day;
    year = month = day = 0;
    if(d >= 0 && d <= 497094) {
/* how many years have rolled by; at worst 366 days in each */
	year = d / 366;
	year += 1640;
	while(dateEncode(++year, 1, 1) <= d) ;
	--year;
	d -= dateEncode(year, 1, 1);
	if(!isLeapYear(year))
	    ndays[2] = 28;
	for(month = 1; month <= 12; ++month) {
	    if(d < ndays[month])
		break;
	    d -= ndays[month];
	}
	day = d + 1;
	ndays[2] = 29;		/* put it back */
    }
    *yp = year;
    *mp = month;
    *dp = day;
}				/* dateDecode */

/* convert a string into a date */
/* return -4 for bad format */
date
stringDate(const char *s, bool yearfirst)
{
    short year, month, day, i, l;
    char delim;
    char buf[12];
    char *t;

    if(!s)
	return nullint;
    l = strlen(s);
    while(l && s[l - 1] == ' ')
	--l;
    if(!l)
	return nullint;
    if(l != 8 && l != 10)
	return -4;
    strncpy(buf, s, l);
    buf[l] = 0;
    delim = yearfirst ? '-' : '/';
    t = strchr(buf, delim);
    if(t)
	strcpy(t, t + 1);
    t = strchr(buf, delim);
    if(t)
	strcpy(t, t + 1);
    l = strlen(buf);
    if(l != 8)
	return -4;
    if(!strcmp(buf, "        "))
	return nullint;
    if(yearfirst) {
	char swap[4];
	strncpy(swap, buf, 4);
	strncpy(buf, buf + 4, 4);
	strncpy(buf + 4, swap, 4);
    }
    for(i = 0; i < 8; ++i)
	if(!isdigit(buf[i]))
	    return -4;
    month = 10 * (buf[0] - '0') + buf[1] - '0';
    day = 10 * (buf[2] - '0') + buf[3] - '0';
    year = atoi(buf + 4);
    return dateEncode(year, month, day);
}				/* stringDate */

/* convert a date into a string, held in a static buffer */
/* cram squashes out the century, delimit puts in slashes */
char *
dateString(date d, int flags)
{
    static char buf[12];
    char swap[8];
    int year, month, day;
    dateDecode(d, &year, &month, &day);
    if(!year)
	strcpy(buf, "  /  /    ");
    else
	sprintf(buf, "%02d/%02d/%04d", month, day, year);
    if(flags & DTCRUNCH)
	strcpy(buf + 6, buf + 8);
    if(flags & YEARFIRST) {
	strncpy(swap, buf, 6);
	swap[2] = swap[5] = 0;
	strcpy(buf, buf + 6);
	if(flags & DTDELIMIT)
	    strcat(buf, "-");
	strcat(buf, swap);
	if(flags & DTDELIMIT)
	    strcat(buf, "-");
	strcat(buf, swap + 3);
    } else if(!(flags & DTDELIMIT)) {
	char *s;
	s = strchr(buf, '/');
	strcpy(s, s + 1);
	s = strchr(buf, '/');
	strcpy(s, s + 1);
    }
    return buf;
}				/* dateString */

char *
timeString(interval seconds, int flags)
{
    short h, m, s;
    char c = 'A';
    static char buf[12];
    if(seconds < 0 || seconds >= 86400)
	strcpy(buf, "  :  :   AM");
    else {
	h = seconds / 3600;
	seconds -= h * 3600L;
	m = seconds / 60;
	seconds -= m * 60;
	s = (short)seconds;
	if(flags & DTAMPM) {
	    if(h == 0)
		h = 12;
	    else if(h >= 12) {
		c = 'P';
		if(h > 12)
		    h -= 12;
	    }
	}
	sprintf(buf, "%02d:%02d:%02d %cM", h, m, s, c);
    }
    if(!(flags & DTAMPM))
	buf[8] = 0;
    if(flags & DTCRUNCH)
	strcpy(buf + 5, buf + 8);
    if(!(flags & DTDELIMIT)) {
	strcpy(buf + 2, buf + 3);
	if(buf[4] == ':')
	    strcpy(buf + 4, buf + 5);
    }
    return buf;
}				/* timeString */

/* convert string into time.
 * Like stringDate, we can return bad hour, bad minute, bad second, or bad format */
interval
stringTime(const char *t)
{
    short h, m, s;
    bool ampm = false;
    char c;
    char buf[12];
    short i, l;
    if(!t)
	return nullint;
    l = strlen(t);
    while(l && t[l - 1] == ' ')
	--l;
    if(!l)
	return nullint;
    if(l < 4 || l > 11)
	return -4;
    strncpy(buf, t, l);
    buf[l] = 0;
    if(buf[l - 1] == 'M' && buf[l - 3] == ' ') {
	ampm = true;
	c = buf[l - 2];
	if(c != 'A' && c != 'P')
	    return -4;
	buf[l - 3] = 0;
	l -= 3;
    }
    if(l < 4 || l > 8)
	return -4;
    if(buf[2] == ':')
	strcpy(buf + 2, buf + 3);
    if(buf[4] == ':')
	strcpy(buf + 4, buf + 5);
    l = strlen(buf);
    if(l != 4 && l != 6)
	return -4;
    if(!strncmp(buf, "      ", l))
	return nullint;
    for(i = 0; i < l; ++i)
	if(!isdigit(buf[i]))
	    return -4;
    h = 10 * (buf[0] - '0') + buf[1] - '0';
    m = 10 * (buf[2] - '0') + buf[3] - '0';
    s = 0;
    if(l == 6)
	s = 10 * (buf[4] - '0') + buf[5] - '0';
    if(ampm) {
	if(h == 12) {
	    if(c == 'A')
		h = 0;
	} else if(c == 'P')
	    h += 12;
    }
    if(h < 0 || h >= 24)
	return -1;
    if(m < 0 || m >= 60)
	return -2;
    if(s < 0 || s >= 60)
	return -3;
    return h * 3600L + m * 60 + s;
}				/* stringTime */

char *
moneyString(money m)
{
    static char buf[20], *s = buf;
    if(m == nullint)
	return "";
    if(m < 0)
	*s++ = '-', m = -m;
    sprintf(s, "$%ld.%02d", m / 100, (int)(m % 100));
    return buf;
}				/* moneyString */

money
stringMoney(const char *s)
{
    short sign = 1;
    long m;
    double d;
    if(!s)
	return nullint;
    skipWhite(&s);
    if(*s == '-')
	sign = -sign, ++s;
    skipWhite(&s);
    if(*s == '$')
	++s;
    skipWhite(&s);
    if(!*s)
	return nullint;
    if(!stringIsFloat(s, &d))
	return -nullint;
    m = (long)(d * 100.0 + 0.5);
    return m * sign;
}				/* stringMoney */

/* Make sure edbrowse is connected to the database */
static bool
ebConnect(void)
{
    const short exclist[] = { EXCSQLMISC, EXCNOCONNECT, 0 };
    if(sql_database)
	return true;
    if(!dbarea) {
	setError(399);
	return false;
    }
    sql_exclist(exclist);
    sql_connect(dbarea, dblogin, dbpw);
    if(rv_lastStatus) {
	setError(400, rv_vendorStatus);
	return false;
    }
    if(!sql_database)
	errorPrint("@sql connected, but database not set");
    return true;
}				/* ebConnect */

void
dbClose(void)
{
    sql_disconnect();
}				/* dbClose */

static char myTab[64];
static const char *myWhere;
static char *scl;		/* select clause */
static int scllen;
static char *wcl;		/* where clause */
static int wcllen;
static char wherecol[COLNAMELEN + 2];
static struct DBTABLE *td;

static void
unexpected(void)
{
    setError(401, rv_vendorStatus);
}				/* unexpected */

static void
buildSelectClause(void)
{
    int i;
    scl = initString(&scllen);
    stringAndString(&scl, &scllen, "select ");
    for(i = 0; i < td->ncols; ++i) {
	if(i)
	    stringAndChar(&scl, &scllen, ',');
	stringAndString(&scl, &scllen, td->cols[i]);
    }
    stringAndString(&scl, &scllen, " from ");
    stringAndString(&scl, &scllen, td->name);
}				/* buildSelectClause */

static bool
buildWhereClause(void)
{
    int i, l, n;
    const char *w = myWhere;
    const char *e;

    wcl = initString(&wcllen);
    wherecol[0] = 0;
    if(stringEqual(w, "*"))
	return true;

    e = strchr(w, '=');
    if(!e) {
	if(!td->key1) {
	    setError(402);
	    return false;
	}
	e = td->cols[td->key1 - 1];
	l = strlen(e);
	if(l > COLNAMELEN) {
	    setError(403, e, COLNAMELEN);
	    return false;
	}
	strcpy(wherecol, e);
	e = w - 1;
    } else if(isdigit(*w)) {
	n = strtol(w, (char **)&w, 10);
	if(w != e) {
	    setError(404);
	    return false;
	}
	if(n == 0 || n > td->ncols) {
	    setError(405, n);
	    return false;
	}
	goto setcol_n;
    } else {
	n = 0;
	if(e - w <= COLNAMELEN) {
	    strncpy(wherecol, w, e - w);
	    wherecol[e - w] = 0;
	    for(i = 0; i < td->ncols; ++i) {
		if(!strstr(td->cols[i], wherecol))
		    continue;
		if(n) {
		    setError(406, wherecol);
		    return false;
		}
		n = i + 1;
	    }
	}
	if(!n) {
	    setError(407, wherecol);
	    return false;
	}
      setcol_n:
	w = td->cols[n - 1];
	l = strlen(w);
	if(l > COLNAMELEN) {
	    setError(403, w, COLNAMELEN);
	    return false;
	}
	strcpy(wherecol, w);
    }

    stringAndString(&wcl, &wcllen, "where ");
    stringAndString(&wcl, &wcllen, wherecol);
    ++e;
    w = e;
    if(!*e) {
	stringAndString(&wcl, &wcllen, " is null");
    } else if((i = strtol(e, (char **)&e, 10)) >= 0 &&
       *e == '-' && (n = strtol(e + 1, (char **)&e, 10)) >= 0 && *e == 0) {
	stringAndString(&wcl, &wcllen, " between ");
	stringAndNum(&wcl, &wcllen, i);
	stringAndString(&wcl, &wcllen, " and ");
	stringAndNum(&wcl, &wcllen, n);
    } else if(w[strlen(w) - 1] == '*') {
	stringAndString(&wcl, &wcllen, lineFormat(" matches %S", w));
    } else {
	stringAndString(&wcl, &wcllen, lineFormat(" = %S", w));
    }

    return true;
}				/* buildWhereClause */

static bool
setTable(void)
{
    static const short exclist[] = { EXCNOTABLE, EXCNOCOLUMN, EXCSQLMISC, 0 };
    int cid, nc, i, part1, part2;
    const char *s = cw->fileName;
    const char *t = strchr(s, ']');
    if(t - s >= sizeof (myTab))
	errorPrint("2table name too long, limit %d characters",
	   sizeof (myTab) - 4);
    strncpy(myTab, s, t - s);
    myTab[t - s] = 0;
    myWhere = t + 1;

    td = cw->table;
    if(td)
	return true;

/* haven't glommed onto this table yet */
    td = findTableDescriptor(myTab);
    if(td) {
	if(!td->types) {
	    buildSelectClause();
	    sql_exclist(exclist);
	    cid = sql_prepare(scl);
	    nzFree(scl);
	    if(rv_lastStatus) {
		if(rv_lastStatus == EXCNOTABLE)
		    setError(408, td->name);
		else if(rv_lastStatus == EXCNOCOLUMN)
		    setError(409);
		else
		    unexpected();
		return false;
	    }
	    td->types = cloneString(rv_type);
	    sql_free(cid);
	}

    } else {

	sql_exclist(exclist);
	cid = sql_prepare("select * from %s", myTab);
	if(rv_lastStatus) {
	    if(rv_lastStatus == EXCNOTABLE)
		setError(408, myTab);
	    else
		unexpected();
	    return false;
	}
	td = newTableDescriptor(myTab);
	if(!td) {
	    sql_free(cid);
	    return false;
	}
	nc = rv_numRets;
	if(nc > MAXTCOLS) {
	    printf("warning, only the first %d columns will be selected\n",
	       MAXTCOLS);
	    nc = MAXTCOLS;
	}
	td->types = cloneString(rv_type);
	td->types[nc] = 0;
	td->ncols = nc;
	for(i = 0; i < nc; ++i)
	    td->cols[i] = cloneString(rv_name[i]);
	sql_free(cid);

	getPrimaryKey(myTab, &part1, &part2);
	if(part1 > nc)
	    part1 = 0;
	if(part2 > nc)
	    part2 = 0;
	td->key1 = part1;
	td->key2 = part2;
    }

    s = strpbrk(td->types, "BT");
    if(s)
	s = strpbrk(s + 1, "BT");
    if(s) {
	setError(410);
	return false;
    }

    cw->table = td;
    return true;
}				/* setTable */

void
showColumns(void)
{
    char c;
    const char *desc;
    int i;

    if(!setTable())
	return;
    printf("table %s", td->name);
    if(!stringEqual(td->name, td->shortname))
	printf(" [%s]", td->shortname);
    i = sql_selectOne("select count(*) from %s", td->name);
    printf(", %d rows\n", i);

    for(i = 0; i < td->ncols; ++i) {
	printf("%d ", i + 1);
	if(td->key1 == i + 1 || td->key2 == i + 1)
	    printf("*");
	printf("%s ", td->cols[i]);
	c = td->types[i];
	switch (c) {
	case 'N':
	    desc = "int";
	    break;
	case 'D':
	    desc = "date";
	    break;
	case 'I':
	    desc = "time";
	    break;
	case 'M':
	    desc = "money";
	    break;
	case 'F':
	    desc = "float";
	    break;
	case 'S':
	    desc = "string";
	    break;
	case 'C':
	    desc = "char";
	    break;
	case 'B':
	    desc = "blob";
	    break;
	case 'T':
	    desc = "text";
	    break;
	default:
	    desc = "?";
	    break;
	}			/* switch */
	printf("%s\n", desc);
    }
}				/* showColumns */

bool
sqlReadRows(const char *filename, char **bufptr)
{
    int cid;
    char *rbuf, *unld, *s;
    int rlen;

    *bufptr = EMPTYSTRING;
    if(!ebConnect())
	return false;
    if(!setTable())
	return false;

    rbuf = initString(&rlen);
    myWhere = strchr(filename, ']') + 1;
    if(*myWhere) {
	if(!buildWhereClause())
	    return false;
	buildSelectClause();
	rv_blobFile = 0;
	cid = sql_prepOpen("%s %0s", scl, wcl);
	nzFree(scl);
	nzFree(wcl);
	while(sql_fetchNext(cid, 0)) {
	    unld = sql_mkunld('\177');
	    if(strchr(unld, '|')) {
		setError(411);
		goto abort;
	    }
	    if(strchr(unld, '\n')) {
		setError(412);
		goto abort;
	    }
	    for(s = unld; *s; ++s)
		if(*s == '\177')
		    *s = '|';
	    s[-1] = '\n';	/* overwrite the last pipe */

/* look for blob column */
	    if(s = strpbrk(td->types, "BT")) {
		int bfi = s - td->types;	/* blob field index */
		int cx = 0;	/* context, where to put the blob */
		int j;
		char *u, *v, *end;
		u = unld;
		for(j = 0; j < bfi; ++j)
		    u = strchr(u, '|') + 1;
		v = strpbrk(u, "|\n");
		end = v + strlen(v);
		if(rv_blobSize) {
		    cx = sideBuffer(0, rv_blobLoc, rv_blobSize, 0, false);
		    nzFree(rv_blobLoc);
		}
		sprintf(myTab, "<%d>", cx);
		if(!cx)
		    myTab[0] = 0;
		j = strlen(myTab);
/* unld is pretty long; I'm just going to assume there is enough room for this */
		memmove(u + j, v, end - v);
		u[j + (end - v)] = 0;
		memcpy(u, myTab, j);
	    }

	    stringAndString(&rbuf, &rlen, unld);
	}
	sql_closeFree(cid);
    }

    *bufptr = rbuf;
    return true;

  abort:
    nzFree(rbuf);
    sql_closeFree(cid);
    return false;
}				/* sqlReadRows */

static char *lineFields[MAXTCOLS];

/* Split a line at pipe boundaries, and make sure the field count is correct */
static bool
intoFields(char *line)
{
    char *s = line;
    int j = 0;
    int c;

    while(1) {
	lineFields[j] = s;
	s = strpbrk(s, "|\n");
	c = *s;
	*s++ = 0;
	++j;
	if(c == '\n')
	    break;
	if(j < td->ncols)
	    continue;
	setError(413);
	return false;
    }

    if(j == td->ncols)
	return true;
    setError(414);
    return false;
}				/* intoFields */

static bool
rowCountCheck(int action, int cnt1)
{
    int cnt2 = rv_lastNrows;

    if(cnt1 == cnt2)
	return true;

    setError(421 + action, cnt1, cnt2);
    return false;
}				/* rowCountCheck */

static int
keyCountCheck(void)
{
    if(!td->key1) {
	setError(415);
	return false;
    }
    return (td->key2 ? 2 : 1);
}				/* keyCountCheck */

/* Typical error conditions for insert update delete */
static const short insupdExceptions[] = { EXCSQLMISC,
    EXCVIEWUSE, EXCREFINT, EXCITEMLOCK, EXCPERMISSION,
    EXCDEADLOCK, EXCCHECK, EXCTIMEOUT, EXCNOTNULLCOLUMN, 0
};

static bool
insupdError(int action, int rcnt)
{
    int rc = rv_lastStatus;
    int msg;

    if(rc) {
	switch (rc) {
	case EXCVIEWUSE:
	    msg = 431;
	    break;
	case EXCREFINT:
	    msg = 424;
	    break;
	case EXCITEMLOCK:
	    msg = 425;
	    break;
	case EXCPERMISSION:
	    msg = 426;
	    break;
	case EXCDEADLOCK:
	    msg = 427;
	    break;
	case EXCNOTNULLCOLUMN:
	    msg = 428;
	    break;
	case EXCCHECK:
	    msg = 429;
	    break;
	case EXCTIMEOUT:
	    msg = 430;
	    break;
	default:
	    setError(416, rv_vendorStatus);
	    return false;
	}

	setError(msg);
	return false;
    }

    return rowCountCheck(action, rcnt);
}				/* insupdError */

bool
sqlDelRows(int start, int end)
{
    int nkeys, ndel, key1, key2, ln;

    if(!setTable())
	return false;

    nkeys = keyCountCheck();
    key1 = td->key1 - 1;
    key2 = td->key2 - 1;
    if(!nkeys)
	return false;

    ndel = end - start + 1;
    ln = start;
    if(ndel > 100) {
	setError(417);
	return false;
    }

/* We could delete all the rows with one statement, using an in(list),
 * but that won't work when the key is two columns.
 * I have to write the one-line-at-a-time code anyways,
 * I'll just use that for now. */
    while(ndel--) {
	char *line = (char *)fetchLine(ln, 0);
	intoFields(line);
	sql_exclist(insupdExceptions);
	if(nkeys == 1)
	    sql_exec("delete from %s where %s = %S",
	       td->name, td->cols[key1], lineFields[key1]);
	else
	    sql_exec("delete from %s where %s = %S and %s = %S",
	       td->name, td->cols[key1], lineFields[key1],
	       td->cols[key2], lineFields[key2]);
	nzFree(line);
	if(!insupdError(0, 1))
	    return false;
	delText(ln, ln);
    }

    return true;
}				/* sqlDelRows */

bool
sqlUpdateRow(pst source, int slen, pst dest, int dlen)
{
    char *d2;			/* clone of dest */
    char *s, *t;
    int j, l1, l2, nkeys, key1, key2;
    char *u1, *u2;		/* pieces of the update statement */
    int u1len, u2len;

/* compare all the way out to newline, so we know both strings end at the same time */
    if(slen == dlen && !memcmp(source, dest, slen + 1))
	return true;

    if(!setTable())
	return false;

    nkeys = keyCountCheck();
    key1 = td->key1 - 1;
    key2 = td->key2 - 1;
    if(!nkeys)
	return false;

    d2 = (char *)clonePstring(dest);
    if(!intoFields(d2)) {
	nzFree(d2);
	return false;
    }

    j = 0;
    u1 = initString(&u1len);
    u2 = initString(&u2len);
    s = (char *)source;

    while(1) {
	t = strpbrk(s, "|\n");
	l1 = t - s;
	l2 = strlen(lineFields[j]);
	if(l1 != l2 || memcmp(s, lineFields[j], l1)) {
	    if(j == key1 || j == key2) {
		setError(418);
		goto abort;
	    }
	    if(td->types[j] == 'B') {
		setError(419);
		goto abort;
	    }
	    if(td->types[j] == 'T') {
		setError(420);
		goto abort;
	    }
	    if(*u1)
		stringAndChar(&u1, &u1len, ',');
	    stringAndString(&u1, &u1len, td->cols[j]);
	    if(*u2)
		stringAndChar(&u2, &u2len, ',');
	    stringAndString(&u2, &u2len, lineFormat("%S", lineFields[j]));
	}
	if(*t == '\n')
	    break;
	s = t + 1;
	++j;
    }

    sql_exclist(insupdExceptions);
    if(nkeys == 1)
	sql_exec("update %s set(%s) = (%s) where %s = %S",
	   td->name, u1, u2, td->cols[key1], lineFields[key1]);
    else
	sql_exec("update %s set(%s) = (%s) where %s = %S and %s = %S",
	   td->name, u1, u2,
	   td->cols[key1], lineFields[key1], td->cols[key2], lineFields[key2]);
    if(!insupdError(2, 1))
	goto abort;

    nzFree(d2);
    nzFree(u1);
    nzFree(u2);
    return true;

  abort:
    nzFree(d2);
    nzFree(u1);
    nzFree(u2);
    return false;
}				/* sqlUpdateRow */

bool
sqlAddRows(int ln)
{
    char *u1, *u2;		/* pieces of the insert statement */
    char *unld, *s;
    int u1len, u2len;
    int j, l, rowid;
    double dv;
    char inp[256];

    if(!setTable())
	return false;

    while(1) {
	u1 = initString(&u1len);
	u2 = initString(&u2len);
	for(j = 0; j < td->ncols; ++j) {
	  reenter:
	    if(strchr("BT", td->types[j]))
		continue;
	    printf("%s: ", td->cols[j]);
	    fflush(stdout);
	    if(!fgets(inp, sizeof (inp), stdin)) {
		puts("EOF");
		ebClose(1);
	    }
	    l = strlen(inp);
	    if(l && inp[l - 1] == '\n')
		inp[--l] = 0;
	    if(stringEqual(inp, ".")) {
		nzFree(u1);
		nzFree(u2);
		return true;
	    }

/* For now, a null field is always excepted. */
/* Someday we may want to check this against the not-null constraint. */
	    if(inp[0] == 0)
		goto goodfield;

/* verify the integrity of the entered field */
	    if(strchr(inp, '|')) {
		puts("please, no pipes in the data");
		goto reenter;
	    }

	    switch (td->types[j]) {
	    case 'N':
		s = inp;
		if(*s == '-')
		    ++s;
		if(stringIsNum(s) < 0) {
		    puts("number expected");
		    goto reenter;
		}
		break;
	    case 'F':
		if(!stringIsFloat(inp, &dv)) {
		    puts("decimal number expected");
		    goto reenter;
		}
		break;
	    case 'C':
		if(strlen(inp) > 1) {
		    puts("one character expected");
		    goto reenter;
		}
		break;
	    case 'D':
		if(stringDate(inp, false) < 0) {
		    puts("date expected");
		    goto reenter;
		}
		break;
	    case 'I':
		if(stringTime(inp) < 0) {
		    puts("time expected");
		    goto reenter;
		}
		break;
	    }

	  goodfield:
	    if(*u1)
		stringAndChar(&u1, &u1len, ',');
	    stringAndString(&u1, &u1len, td->cols[j]);
	    if(*u2)
		stringAndChar(&u2, &u2len, ',');
	    stringAndString(&u2, &u2len, lineFormat("%S", inp));
	}
	sql_exclist(insupdExceptions);
	sql_exec("insert into %s (%s) values (%s)", td->name, u1, u2);
	nzFree(u1);
	nzFree(u2);
	if(!insupdError(1, 1)) {
	    printf("Error: ");
	    showError();
	    continue;
	}
/* Fetch the row just entered;
its serial number may have changed from 0 to something real */
	rowid = rv_lastRowid;
	buildSelectClause();
	sql_select("%s where rowid = %d", scl, rowid, 0);
	nzFree(scl);
	unld = sql_mkunld('|');
	l = strlen(unld);
	unld[l - 1] = '\n';	/* overwrite the last pipe */
	if(!addTextToBuffer((pst) unld, l, ln))
	    return false;
	++ln;
    }

/* This pointis not reached; make the compilerhappy */
    return true;
}				/* sqlAddRows */


/*********************************************************************
Sync up two tables, or corresponding sections of two tables.
These are usually equischema tables in parallel databases or machines.
This isn't used by edbrowse; it's just something I wrote,
and I thought you might find it useful.
It follows the C convention of copying the second argument
to the first, like the string and memory functions,
rather than the shell convention of copying (cp) the first argument to the second.
Hey - why have one standard, when you can have two?
*********************************************************************/

static const char *synctable;	/* table being sync-ed */
static const char *synckeycol;	/* key column */
static const char *sync_clause;	/* additional clause, to sync only part of the table */

static int
syncup_comm_fn(char action, char *line1, char *line2, int key)
{
    switch (action) {
    case '<':			/* delete */
	sql_exec("delete from %s where %s = %d %0s",
	   synctable, synckeycol, key, sync_clause);
	break;
    case '>':			/* insert */
	sql_exec("insert into %s values(%s)", synctable, line2);
	break;
    case '*':			/* update */
	sql_exec("update %s set * = (%s) where %s = %d %0s",
	   synctable, line2, synckeycol, key, sync_clause);
	break;
    }				/* switch */
    return 0;
}				/* syncup_comm_fn */

/* make table1 look like table2 */
void
syncup_table(const char *table1, const char *table2,	/* the two tables */
   const char *keycol,		/* the key column */
   const char *otherclause)
{
    char stmt1[200], stmt2[200];
    int len;

    synctable = table1;
    synckeycol = keycol;
    sync_clause = otherclause;
    len = strlen(table1);
    if((int)strlen(table2) > len)
	len = strlen(table2);
    if(otherclause)
	len += strlen(otherclause);
    len += strlen(keycol);
    if(len + 30 > sizeof (stmt1))
	errorPrint
	   ("2constructed select statement in syncup_table() is too long");

    if(otherclause) {
	while(*otherclause == ' ')
	    ++otherclause;
	if(strncmp(otherclause, "and ", 4) && strncmp(otherclause, "AND ", 4))
	    errorPrint
	       ("2restricting clause in syncup_table() does not start with \"and\".");
	sprintf(stmt1, "select * from %s where %s order by %s", table1,
	   otherclause + 4, keycol);
	sprintf(stmt2, "select * from %s where %s order by %s", table2,
	   otherclause + 4, keycol);
    } else {
	sprintf(stmt1, "select * from %s order by %s", table1, keycol);
	sprintf(stmt2, "select * from %s order by %s", table2, keycol);
    }

    cursor_comm(stmt1, stmt2, keycol, (fnptr) syncup_comm_fn, 0);
}				/* syncup_table */
