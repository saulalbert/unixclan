/**********************************************************************
	"Copyright 1990-2018 Brian MacWhinney. Use is subject to Gnu Public License
	as stated in the attached "gpl.txt" file."
*/


#define CHAT_MODE 0

#include "cu.h"

#if !defined(UNX)
#define _main freqmerge_main
#define call freqmerge_call
#define getflag freqmerge_getflag
#define init freqmerge_init
#define usage freqmerge_usage
#endif

#define pr_result freqmerge_pr_result
#define IS_WIN_MODE FALSE
#include "mul.h" 

struct words {
    char *word;
    int count;
    struct words *nextWord;
    struct words *prevWord;
};

static long total_words_cnt;
static char fm_sort = 0;
static struct words *fm_root;

void usage()	{		/* print proper usage and exit */
    puts("FREQMERG combines outputs of freq program");
    printf("Usage : freqmerg [oN %s] filename(s)\n",mainflgs());
    puts("-o : sort output by descending frequency");
    puts("-o1: sort output by reverse alphabetic order");
    mainusage(TRUE);
}

void init(char f) {
    if (f) {
		fm_sort = 0;
		chatmode = 0;
		combinput = TRUE;
		// defined in "mmaininit" and "globinit" - nomap = TRUE;
    }
    if (f || !combinput) {
		fm_root = NULL;
		total_words_cnt = 0L;
    }
}

static void fm_remove_endspace(char *line) {
    register int i;

    i = strlen(line) - 1;
    if (line[i] == '\n') line[i]=EOS;
    else fputs("Line too long  : end ignored.\n",stderr);
    for (i--; line[i] == ' ' && i > 0; i--) ;
    line[++i] = EOS;
}

static void fm_remove_startspace(char *line) { 
    char *beg;

    for (beg=line; *line && (*line == ' ' || *line == '\t'); line++);
    if (beg != line) strcpy(beg,line);
}

static void fm_AddWord(char *word, int cnt) {
    int cond;
    long cur, low, high, mid;
    struct words *midWord, *tWord;
	
    cur = 0;
    low = 0;
    high = total_words_cnt - 1;
    midWord = fm_root;
    while (low <= high) {
		mid = (low+high) / 2;
		if (cur < mid) {
			for (; cur < mid; cur++) 
				midWord = midWord->nextWord;
		} else if (cur > mid) {
			for (; cur > mid; cur--) 
				midWord = midWord->prevWord;
		}
		if (midWord == NULL) {
			fprintf(stderr, "Freqmerge: Internal error!\n");
			cutt_exit(1);
		}
		if ((cond=strcmp(word, midWord->word)) < 0)
			high = mid - 1;
		else if (cond > 0)
			low = mid + 1;
		else {
			midWord->count += cnt;
			return;
		}
    }
    if (midWord == NULL) {
		fm_root = NEW(struct words);
		if (fm_root == NULL) out_of_mem();
		tWord = fm_root;
		tWord->nextWord = NULL;
		tWord->prevWord = NULL;
    } else {
		tWord = NEW(struct words);
		if (tWord == NULL) out_of_mem();
		if (strcmp(word, midWord->word) < 0) {
			tWord->nextWord = midWord;
			tWord->prevWord = midWord->prevWord;
			midWord->prevWord = tWord;
			if (tWord->prevWord != NULL) tWord->prevWord->nextWord = tWord;
			if (midWord == fm_root) fm_root = tWord;
		} else {
			tWord->nextWord = midWord->nextWord;
			tWord->prevWord = midWord;
			midWord->nextWord = tWord;
			if (tWord->nextWord != NULL) tWord->nextWord->prevWord = tWord;
		}
    }
    tWord->word = (char *)malloc(strlen(word)+1);
    if (tWord->word == NULL) out_of_mem();
    strcpy(tWord->word, word);
    tWord->count = cnt;
    total_words_cnt++;
}

static struct words *freeWordsList(struct words *p, char isFreeWord) {
	struct words *t;

	while (p != NULL) {
		t = p;
		p = p->nextWord;
		if (isFreeWord && t->word != NULL)
			free(t->word);
		free(t);
	}
	return(p);
}

static struct words *sortFreqOrder(struct words *root) {
    struct words *fm_root, *nt, *tnt, *tr;

	fm_root = NULL;
	for (tr=root; tr != NULL; tr=tr->nextWord) {
		if (fm_root == NULL) {
			fm_root = NEW(struct words);
			nt = fm_root;
			nt->nextWord = NULL;
			nt->prevWord = NULL;
		} else {
			tnt= fm_root;
			nt = fm_root;
			while (1) {
				if (nt == NULL) {
					break;
				} else if (tr->count > nt->count) {
					break;
				}
				tnt = nt;
				nt = nt->nextWord;
			}
			if (nt == NULL) {
				tnt->nextWord = NEW(struct words);
				nt = tnt->nextWord;
				nt->prevWord = NULL;
				nt->nextWord = NULL;
			} else if (nt == fm_root) {
				fm_root = NEW(struct words);
				fm_root->prevWord = NULL;
				fm_root->nextWord = nt;
				nt = fm_root;
			} else {
				nt = NEW(struct words);
				nt->prevWord = NULL;
				nt->nextWord = tnt->nextWord;
				tnt->nextWord = nt;
			}
		}
		nt->word = tr->word;
		nt->count = tr->count;
	}
	root = freeWordsList(root, FALSE);
	return(fm_root);
}

static void pr_result(void) {
    struct words *t;
	
    if (fm_sort == 1) {
		if (fm_root == NULL)
			return;
		fm_root = sortFreqOrder(fm_root);
		for (t=fm_root; t != NULL; t=t->nextWord) {
			if (!onlydata)
				fprintf(fpout, "%3d ", t->count);
			fprintf(fpout,"%s\n", t->word);
		}
    } else if (fm_sort == 2) {
		if (fm_root == NULL) return;
		for (t=fm_root; t->nextWord != NULL; t=t->nextWord) ;
		for (; t != NULL; t=t->prevWord) {
			if (!onlydata)
				fprintf(fpout, "%3d ", t->count);
			fprintf(fpout,"%s\n", t->word);
		}
    } else {
		for (t=fm_root; t != NULL; t=t->nextWord) {
			if (!onlydata)
				fprintf(fpout, "%3d ", t->count);
			fprintf(fpout,"%s\n", t->word);
		}
    }
	fm_root = freeWordsList(fm_root, TRUE);
}

void call(void) {
    int i, cnt;
    char nline[BUFSIZ], StartCount = TRUE;
	
    while (fgets_cr(nline,BUFSIZ,fpin) != NULL) {
		if (strncmp(nline, "----------", 10) == 0)
			StartCount = FALSE;
		if (strncmp(nline, "From file ", 10) == 0 || strncmp(nline, "Speaker: *", 10) == 0 || strncmp(nline, ";%* ", 4) == 0) {
			StartCount = TRUE;
			continue;
		}
		if (!StartCount)
			continue;
		fm_remove_startspace(nline);
		fm_remove_endspace(nline);
		if (!nomap)
			uS.lowercasestr(nline, &dFnt, MBF);
		if (!isdigit(*nline)) {
			if (!isalpha(*nline) || strchr(nline, ' ') != NULL || strchr(nline, '\t') != NULL)
				continue;
			cnt = 1;
		} else {
			cnt = atoi(nline);
		}
		for (i=0; isdigit(nline[i]); i++) ;
		for (; nline[i] == ' ' || nline[i] == '\t'; i++) ;
		if (nline[i] == EOS)
			continue;
		if (i > 0)
			strcpy(nline, nline+i);
		if (exclude(nline))
			fm_AddWord(nline,cnt);
    }
    if (!combinput)
		pr_result();
}

void getflag(char *f, char *f1, int *i) {
    f++;
    switch(*f++) {
	case 'o':
		if (*f == EOS || *f == '0')
			fm_sort = 1;
		else if (*f == '1')
			fm_sort = 2;
		else {
			fprintf(stderr,"Invalid argument for option: %s\n", f-2);
			cutt_exit(0);
		}
		break;
	default:
		maingetflag(f-2,f1,i);
		break;
	}
}

CLAN_MAIN_RETURN main(int argc, char *argv[]) {
    isWinMode = IS_WIN_MODE;
    chatmode = CHAT_MODE;
    CLAN_PROG_NUM = FREQMERGE;
    OnlydataLimit = 1;
    UttlineEqUtterance = TRUE;
    bmain(argc,argv,pr_result);
}
