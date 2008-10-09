#include "main.h"

/* ***** BEGIN LICENSE BLOCK *****
 *
 * BBN Address and AS Number PKI Database/repository software
 * Version 1.0
 *
 * US government users are permitted unrestricted rights as
 * defined in the FAR.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT
 * WARRANTY OF ANY KIND, either express or implied.
 *
 * Copyright (C) BBN Technologies 2007.  All Rights Reserved.
 *
 * Contributor(s):  Peiter "Mudge" Zatko
 *
 * ***** END LICENSE BLOCK ***** */

/*
  $Id$
*/
#define MANPASS 0
#define TRUSTPASS 1
#define GENPASS 2
#define NUMPASSES 3
struct name_list
  {
  char *namep;
  int state;
  struct name_list *nextp;
  };

static void free_name_list(struct name_list *rootlistp)
  {
  struct name_list *currp, *nextp;
  for (currp = rootlistp; currp; currp = nextp)
    {
    free(currp->namep);
    nextp = currp->nextp;
    if (currp > rootlistp) free(currp);
    }
  }

// return an (allocated) downcased version of the string
static char *downcase(char *str)
{
  char *p = calloc(1, 1 + strlen(str)), *q;
  strcpy(p, str);
  for (q = p; *q; ++q)
    if (isupper(*q))
      *q = tolower(*q);
  return p;
}

// does this have TRUST in its name?
static int isTrust(char *namep)
  {
  int ret = 0;
  char *down = downcase(namep);
  if (strstr(namep, "trust")) ret = 1;
  free(down);
  return ret;
  }

// see if it's a manifest based on its name
static int isManifest(char *namep)
  {
  int ret = 0;
  char *down = downcase(namep);
  if (strstr(down, ".man") || strstr(down, ".mft") || strstr(down, "manifest"))
    ret = 1;
  free(down);
  return (ret);
  }

// is it something that should be on a manifest (i.e. it's not a manifest)?
static int mustManifest(char *namep)
  {
  /*
  Returns 1 for certs, crls  or roas
          0 for everything else
 */
  int ret = 0;

  if (isTrust(namep)) {
    ret = 0;
  } else {
    char *down = downcase(namep);
    if (strstr(down, "der") || strstr(down, "cer") || strstr(down, "pem") ||
	strstr(down, "crl") || strstr(down, "roa")) {
      ret = 1;
    }
    free(down);
  }
  return(ret);
  }

static int check_manifest(struct ROA *roap, char *mfname, char *topDir,
  char *midDir,  struct name_list *rootp)
  {
  int sig_err = 0;
  struct name_list *curr_namep;
  for (curr_namep = rootp; curr_namep->nextp; curr_namep = curr_namep->nextp);
     // leaves curr_namep at the last one that has no name, which may be rootp
  struct badfile **badfilespp = (struct badfile **)0;
  struct Manifest *manp = &roap->content.signedData.encapContentInfo.eContent.
    manifest;
  char *refDir;
  if (midDir && *midDir)
    {
    refDir = (char *)calloc(1, strlen(topDir) + strlen(midDir) + 4);
    strcat(strcat(strcpy(refDir, topDir), "/"), midDir);
    }
  else refDir = topDir;

  if ((sig_err = manifestValidate2(roap, refDir, &badfilespp)) < 0 && !badfilespp)
    {
    char *cc;
    for (cc = mfname; *cc > ' '; cc++);
    *cc = 0;
    if (sig_err == ERR_SCM_INVALSIG) cc = "signature";
    else if (sig_err == ERR_SCM_BADCT) cc = "syntax";
    else if (sig_err == ERR_SCM_BADNUMCERTS) cc = "number of certificates";
    else if (sig_err == ERR_SCM_BADVERS) cc = "version";
    else if (sig_err == ERR_SCM_NOTEE) cc = "non-EE certificate";
    else if (sig_err == ERR_SCM_BADDATES) cc = "invalid dates";
    else cc = "undefined";
    fprintf(stderr, "%s error in manifest %s\n", cc, mfname);
    if (midDir && *midDir) free(refDir);
    return -1;
    }
     // make list of all certificates, CRLs and ROAs  in manifest
  struct FileAndHash *fahp;
  for (fahp = (struct FileAndHash *)member_casn(&roap->content.signedData.encapContentInfo.
    eContent.manifest.fileList.self, 0); fahp;
    fahp = (struct FileAndHash *)next_of(&fahp->self))
    {
    char *fname;
    if (readvsize_casn(&fahp->file, (uchar **)&fname) > 0) 
      {
      if (mustManifest(fname))
        {  //  add it to the manifested list
        curr_namep->namep = fname;
        curr_namep->state = 1;  // until proven otherwise
        if (badfilespp)    // check if it is on the "bad" list
          {
          struct badfile **bpp;
          for (bpp  = badfilespp; 
	       *bpp && strcmp(fname, (*bpp)->fname) != 0; 
	       bpp++)
	    ;
          if (*bpp)  // mark its state in namelist
            {
            int err = (*bpp)->err;
            curr_namep->state = (err == ERR_SCM_COFILE)? 0: err;
            }
          }
        curr_namep->nextp = (struct name_list *)calloc(1, sizeof(struct name_list));
        curr_namep = curr_namep->nextp;
        }
      }
    }
  if (badfilespp) free_badfiles(badfilespp);
  if (midDir && *midDir) free(refDir);
  time_t now = time ((time_t *)0); 
  ulong mhi, mlo;
  read_casn_time(&manp->thisUpdate, &mlo);
  read_casn_time(&manp->nextUpdate, &mhi);
  if (mlo > now || mhi < now) return 0;
  return 1;
  }


static char *makeCDStr(unsigned int *retlenp, char *dir)
{
  char *buf;
  char *ret;

  *retlenp = 0;
  buf = (char *)calloc(PATH_MAX+6, sizeof(char));
  if ( buf == NULL )
    return(NULL);
  if (dir == NULL) {
    ret = getcwd(buf+2, PATH_MAX+1);
    if ( ret == NULL )
    {
      free((void *)buf);
      return(NULL);
    }
  } else {
    strncpy (buf+2, dir, PATH_MAX+1);
  }
  buf[0] = 'C';
  buf[1] = ' ';
  (void)strncat(buf, "\r\n", 2);
  *retlenp = strlen(buf);
  return(buf);
}

static int multi_match(struct name_list *rootp, char *fname, int fnamelth)
  {
  int rootlth = strlen(rootp->namep);
  char *s;
  if (fnamelth > rootlth) return 0;
  for (s = &rootp->namep[rootlth - fnamelth]; s >= rootp->namep; s--)
    {
    if (!strcmp(s, fname)) return 1;
    }
  return 0;
  }  

static int wasManifested(struct name_list *rootp, char *fnamep, int ee)
  {   // ee means to match the tail end of the paths in rootlist with multi_match
  char *b;
  if (!rootp || !rootp->namep) return 0;
  for (b = fnamep; *b > ' '; b++);
  char x = *b;
  *b = 0;
  char *t;
  for (t = fnamep; *t; t++);  // chop off all but last part of filename
  for (t--; t > fnamep && *t != '/'; t--);
  if (*t == '/') fnamep = ++t;
  int ansr = 0;
  if ((ee && !multi_match(rootp, fnamep, b - fnamep)) ||
    (!ee && !strcmp(rootp->namep, fnamep))) ansr = 1;
  else for (rootp = rootp->nextp; rootp->namep; rootp = rootp->nextp)
    {
    if ((ee && !multi_match(rootp, fnamep, b - fnamep)) ||
      (!ee && !strcmp(rootp->namep, fnamep))) 
      {
      ansr = 1;
      break;
      }
    }
  *b = x;
  if (ansr == 1) return (rootp->state < 0)? rootp->state: 1;
  return ansr;
  }

static char *appendState(char *sendStr, int state, unsigned int *retlenp)
  {
  char *retStr = (char *)realloc(sendStr, strlen(sendStr) + 4);
  char *a;
  for (a = &retStr[2]; *a >= ' '; a++);
  *a = 0;
  sprintf(a, " %d\r\n", state);
  *retlenp = strlen(retStr);
  return retStr;
  }

int main(int argc, char *argv[])
  {

  /* tflag = tcp, uflag = udp, nflag = do nothing, just print
     what you would have done, {w,e,i}flag = {warning,error,
     information} flags for what will be sent, ch is for getopt */

  int tflag, uflag, nflag, fflag, sflag, ch, i;
  int portno;
  unsigned int retlen;
  FILE *fp;
  char *sendStr;
  char *topDir = NULL;
  struct write_port wport;
  char flags;  /* our warning flags bit fields */

  tflag = uflag = nflag = fflag = sflag = ch = 0;
  portno = retlen = 0;
  flags = 0;

  memset((char *)&wport, '\0', sizeof(struct write_port));

  if (argc == 2 && *argv[1] != '-') // process a script file
    {
    char *cc, *cx, *buf, *e;
    int fd, bufsize;
    if ( (fd = open(argv[1], O_RDONLY)) < 0 || (bufsize = lseek(fd, 0, SEEK_END)) <= 0 ||
      (buf = (char *)calloc(1, bufsize + 6)) == 0 || lseek(fd, 0, SEEK_SET) != 0 ||
      read(fd, buf, bufsize + 4) != bufsize)
      {
      fprintf(stderr, "failed to open %s\n", argv[1]);
      exit(1);
      }
    for (cc = buf, e = &buf[bufsize]; cc < e; cc++)  // null out white space
      {
      if (*cc <= ' ') *cc = 0;
      }
    for (cc = buf; cc < e; )
      {
      while (*cc == 0 && cc < e) cc++;
      if (*cc++ == '-')
        {
        if (*cc == 'e') flags |= ERROR_FLAG;
        else if (*cc == 'i') flags |= INFO_FLAG;
        else if (*cc == 'n') nflag = 1;
        else if (*cc == 's') sflag = 1;
        else if (*cc == 'w') flags |= WARNING_FLAG;
        else if (*cc == 'd'|| *cc == 'f' || *cc == 't' || *cc == 'u')
          {
          for (cx = &cc[1]; *cx == 0 && cx < e; cx++);
          if (*cc == 'd') topDir = strdup(cx);
          else if (*cc == 'f')
            {
            fflag = 1;
            char *ce;
            for (ce = cx; *ce > ' '; ce++);
            *ce = 0;
            if (!(fp = fopen(cx, "r")))
               {
               fprintf(stderr, "failed to open %s\n", cx);
               exit(1);
               }
            }
          else if (*cc == 't') { tflag = 1; portno = atoi(cx); }
          else if (*cc == 'u') { uflag = 1; portno = atoi(cx); }
          for (cc = cx; *cc > ' '; cc++);
          }
        else usage(argv[0]);
        cc++;
        }
      while (*cc == 0 && cc < e) cc++;
      }
    free(buf);
    }
  else if (argc > 2 && *argv[1] != '-')
    {
    fprintf(stderr, "Too many script files: %s\n", argv[2]);
    exit(1);
    } 
  else
    {
    while ((ch = getopt(argc, argv, "t:u:f:nweid:sh")) != -1)
      {
      switch (ch)
        {
        case 't':  /* TCP flag */
          tflag = 1;
          portno = atoi(optarg);
          break;
        case 'u':  /* UDP flag */
          uflag = 1;
          portno = atoi(optarg);
          break;
        case 'n': /* do nothing flag - print what messages would have been
                     sent */
          nflag = 1;
          break;
        case 'w': /* create warning message(s) */
          flags = flags | WARNING_FLAG;
          break;
        case 'e': /* create error message(s) */
          flags = flags | ERROR_FLAG;
          break;
        case 'i': /* create information message(s) */
          flags = flags | INFO_FLAG;
          break;
        case 'f': /* log file to read and parse */
          fflag = 1;
          fp = fopen(optarg, "r");
          if (!fp) {
            fprintf(stderr, "failed to open %s\n", optarg);
            exit(1);
          }
          break;
        case 'd':
  	topDir = strdup (optarg);
  	break;
        case 's': /* synchronize */
  	sflag = 1;
  	break;
        case 'h': /* help */
        default:
          usage(argv[0]);
          break;
        }
      }
    }

  /* test for necessary flags */
  if (!fflag) {
    fprintf(stderr, "please specify rsync logfile with -f. Or -h for help\n");
    exit(1);
  }

  /* test for conflicting flags here... */
  if (tflag && uflag) {
    fprintf(stderr, "choose either tcp or udp, not both. or -h for help\n");
    exit(1);
  }

  if (!tflag && !uflag && !nflag) { /* if nflag then we don't care */
    fprintf(stderr, "must choose tcp or udp, or specify -n. -h for help\n");
    exit(1);
  }

  /* setup sockets... */
  if (!nflag) {
    if (tflag) {
      if (tcpsocket(&wport, portno) != TRUE) {
        fprintf(stderr, "tcpsocket failed...\n");
        exit(-1);
      }
    } else if (uflag) {
      if (udpsocket(&wport, portno) != TRUE) {
        fprintf(stderr, "udpsocket failed...\n");
        exit(-1);
      }
    }
  } else {
    wport.out_desc = STDOUT_FILENO;
    wport.protocol = LOCAL;
  }

  /* set the global pointer to the wport struct here - don't
     know if this will cause a fault or not. Can't remember.
     Doing this to be able to communicate with the server
     through the descriptor after a sigint or other signal
     has been caught. */
  global_wport = &wport;

  if (setup_sig_catchers() != TRUE) {
    fprintf(stderr, "failed to setup signal catchers... bailing.\n");
    exit(FALSE);
  }

  /****************************************************/
  /* Make the Start String                            */
  /* send the Start String                            */
  /* free it                                          */
  /****************************************************/
  sendStr = makeStartStr(&retlen);
  if (!sendStr) {
    fprintf(stderr, "failed to make Start String... bailing...\n");
    exit(1);
  }

  outputMsg(&wport, sendStr, retlen);
  retlen = 0;
  free(sendStr);

  /****************************************************/
  /* Make the Directory String                        */
  /* send the Directory String                        */
  /* free it                                          */
  /****************************************************/
  sendStr = makeCDStr(&retlen, topDir);
  if (!sendStr) {
    fprintf(stderr, "failed to make Directory String... bailing...\n");
    exit(1);
  }

  outputMsg(&wport, sendStr, retlen);
  retlen = 0;
  free(sendStr);

  /****************************************************/
  /* do the main parsing and sending of the file loop */
  /****************************************************/
  retlen = 0;
  struct name_list rootlist;
  struct ROA roa;
  ROA(&roa, (ushort)0);
  char holding[PATH_MAX+40];
  char curr_dir[PATH_MAX];

  char *bc = fgets(holding, PATH_MAX, fp);
  if (holding[0] != 'c' || holding[1] != 'd')
    {
    fprintf(stderr, "Log doesn't start with 'cd' statement... bailing\n");
    exit(1);
    }
  if (holding[10] != '.' || holding[11] != '/') 
    {
    strcpy(curr_dir, &holding[10]);
    char *x;
    for (x = curr_dir; *x > ' '; x++);
    if (x[-1] == '/') x[-1] = 0;
    else *x = 0;
    }
  int startdir, nextdir;
  for (nextdir = ftell(fp); bc != NULL; ) // start just after cd stmt
    {
    startdir = nextdir;
    fseek(fp, startdir, SEEK_SET);
    memset(&rootlist, 0, sizeof(rootlist));
    for (i = MANPASS; i < NUMPASSES; i++)
      {
      while ((bc = fgets(holding, PATH_MAX, fp)) != NULL)
        {
        if (!(sendStr = getMessageFromString(holding, (unsigned int)
          strlen(holding),
          &retlen, flags))) continue;
        if (!strncmp(sendStr, "I cd", 4)) break;  // break out of while
        int have_manifest = isManifest(sendStr);
        char *fname = (char *)calloc(1, strlen(topDir) + strlen(sendStr) + 8);
          strcat(strcat(strcpy(fname, topDir), "/"), &sendStr[2]);
        char *b;
        for (b = fname; *b >= ' '; b++); // trim off CRLF
        *b = 0;
        struct stat tstat;
        int exists = (stat(fname, &tstat) == 0);
           // if doing manifests and it's a manifest, check contents & 
           // note file names
        if (i == MANPASS  && have_manifest != 0 && exists)
          {
          if (get_casn_file(&roa.self, fname, 0) < 0)
            {
            fprintf(stderr, "invalid manifest %s\n", fname);
            continue;
            }
          int ansr;
          if ((ansr = check_manifest(&roa, &sendStr[2], topDir, curr_dir, &rootlist)) 
            >= 0)
            {
            sendStr = appendState(sendStr, ansr, &retlen);  // send manifest
            outputMsg(&wport, sendStr, retlen);
            }
          }
          // else if doing trusts and it's a trust, send message
        else if (i == TRUSTPASS && isTrust(sendStr) && exists)
          {
          outputMsg(&wport, sendStr, retlen);
          }
          // else if doing other stuff and it's not a trust nor a manifest
        else if (i == GENPASS && !isTrust(sendStr) && !have_manifest)
          {
          if (exists == 0 && (*sendStr == 'A' || *sendStr == 'U' || 
            *sendStr == 'R')) 
            fprintf(stderr, "cannot find %s", &sendStr[2]);
          else
            {  
            if (mustManifest(&sendStr[2]))  // if it should be on a manifest
              {
              sendStr = appendState(sendStr, 
                wasManifested(&rootlist, &sendStr[2], 0),
                &retlen);
              }
            outputMsg(&wport, sendStr, retlen);
            }
          }
        retlen = 0;
        if (fname) free(fname);
        free(sendStr);
        memset(holding, '\0', sizeof(holding));
        }
      nextdir = ftell(fp);
      fseek (fp, startdir,  SEEK_SET);
      }
    free_name_list(&rootlist);
    strcpy(curr_dir, &sendStr[12]);
    char *x;
    for (x = curr_dir; *x > ' '; x++);
    if (x[-1] == '/') x[-1] = 0;
    else *x = 0;
    }
  free (topDir);

  char *c;
  if (sflag) {
    outputMsg(&wport, "Y \r\n", 4);
    recv(wport.out_desc, &c, 1, MSG_WAITALL);
  }

  /****************************************************/
  /* Make the End String                              */
  /* send the End String                              */
  /* free it                                          */
  /****************************************************/
  sendStr = makeEndStr(&retlen);
  if (!sendStr) {
    fprintf(stderr, "failed to make End String... bailing...\n");
    exit(1);
  }
  outputMsg(&wport, sendStr, retlen);
  free(sendStr);

  /* close descriptors etc. */
  if (wport.protocol != LOCAL) {
    close(wport.out_desc);
  }
  return(0);
}
