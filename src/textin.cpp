/**********************************************************************
	"Copyright 1990-2018 Brian MacWhinney. Use is subject to Gnu Public License
	as stated in the attached "gpl.txt" file."
*/


#define CHAT_MODE 0

#include "cu.h"
/* // NO QT
#ifdef _WIN32
	#include <TextUtils.h>
#endif
*/

#if !defined(UNX)
#define _main textin_main
#define call textin_call
#define getflag textin_getflag
#define init textin_init
#define usage textin_usage
#endif

#define IS_WIN_MODE FALSE
#include "mul.h" 

extern char OverWriteFile;

static char isIndBlankHeaders;
static char isLineUtt;
static char isLowerCase;
static char isHeritage;

void usage() {
	puts("TEXTIN converts plain text file into CHAT file");
	printf("Usage: textin [cN %s] filename(s)\n",mainflgs());
	puts("+c0: insert @Blank and @Indent headers when appropriate");
	puts("+c1: each line is an utterance regardless of presence of utterance delimiter");
	puts("+c2: convert first capitalized word of utterance and quotation to lower case");
	puts("+c3: do not insert \"@Options: heritage\" header");
	mainusage(TRUE);
}

void init(char s) {
	if (s) {
		isIndBlankHeaders = FALSE;
		isLineUtt = FALSE;
		isLowerCase = FALSE;
		isHeritage = TRUE;
		stout = FALSE;
		onlydata = 3;
		OverWriteFile = TRUE;
		AddCEXExtension = ".cha";
	}
}

CLAN_MAIN_RETURN main(int argc, char *argv[]) {
	isWinMode = IS_WIN_MODE;
	chatmode = CHAT_MODE;
	CLAN_PROG_NUM = TEXTIN;
	OnlydataLimit = 0;
	UttlineEqUtterance = FALSE;
	bmain(argc,argv,NULL);
}

void getflag(char *f, char *f1, int *i) {
	f++;
	switch(*f++)
	{
		case 'c':
			if (*f == '0') {
				isIndBlankHeaders = TRUE;
			} else if (*f == '1') {
				isLineUtt = TRUE;
			} else if (*f == '2') {
				isLowerCase = TRUE;
			} else if (*f == '3') {
				isHeritage = FALSE;
			} else {
				fprintf(stderr, "Please choose one of for +c option: '0', '1', '2' or '3'\n");
				cutt_exit(0);
			}
			break;
		default:
			maingetflag(f-2,f1,i);
			break;
	}
}

static void lowercaseTier(char *line) {
	if (isupper((unsigned char)*line) && (*line != 'I' || isalnum(*(line+1))))
		*line = (char)tolower((unsigned char)*line);
	for (; *line != EOS; line++) {
		if (UTF8_IS_LEAD((unsigned char)*line) && *line == (char)0xE2) {
			if (*(line+1) == (char)0x80 && *(line+2) == (char)0x9C) {
				if (isupper((unsigned char)*(line+3)))
					*(line+3) = (char)tolower((unsigned char)*(line+3));
			}
		}
	}
}

static void addTab(char *line) {
	for (; *line != EOS; line++) {
		if (*line == ':') {
			*(line+1) = '\t';
			break;
		}
	}
}

static void addUttDel(char *line) {
	int i;

	i = strlen(line) - 1;
	for (; i >= 0; i--) {
		if (line[i] == '.' || line[i] == '!' || line[i] == '?')
			break;
	}
	if (i < 0)
		strcat(line, " .");
}

void call() {		/* this function is self-explanatory */
	register long pos;
	register int cr;
	register int i;
	char bl, qf;

	if (fgets_cr(utterance->line, UTTLINELEN, fpin) == NULL)
		return;
	if (uS.partcmp(utterance->line,FONTHEADER,FALSE,FALSE)) {
		cutt_SetNewFont(utterance->line, EOS);
		fputs(utterance->line, fpout);
		if (fgets_cr(utterance->line, UTTLINELEN, fpin) == NULL)
			return;
	}
	pos = 0L;
	fprintf(fpout, "%s\n", UTF8HEADER);
	fprintf(fpout, "@Begin\n");
	fprintf(fpout, "@Languages:	eng\n");
	fprintf(fpout, "@Participants:\tTXT Text\n");
	if (isHeritage)
		fprintf(fpout, "@Options:\theritage\n");
	fprintf(fpout, "@ID:	eng|text|TXT|||||Text|||\n");
	i = 0;
	cr = 0;
	bl = TRUE;
	qf = FALSE;
	*uttline = EOS;
	do {
		if (utterance->line[pos] == '\t')
			utterance->line[pos] = ' ';
		if (utterance->line[pos] == '\n') {
			if (bl) {
				i = cr;
				if (isIndBlankHeaders)
					fprintf(fpout, "@Blank\n");
			}
			bl = TRUE;
			cr = i;
		} else if (utterance->line[pos] != ' ')
			bl = FALSE;

		if (utterance->line[pos] == '"') {
			if (!qf) {
				uttline[i++] = (char)0xe2;
				uttline[i++] = (char)0x80;
				uttline[i++] = (char)0x9c;
				qf = TRUE;
			} else {
				uttline[i++] = (char)0xe2;
				uttline[i++] = (char)0x80;
				uttline[i++] = (char)0x9d;
				qf = FALSE;
			}
		} else
			uttline[i++] = utterance->line[pos];
		if (uS.partwcmp(uttline, UTF8HEADER) && i >= 5) {
			i -= 5;
			pos++;
		} else if (uS.partwcmp(utterance->line+pos, FONTMARKER)) {
			cutt_SetNewFont(utterance->line,']');
			uttline[i-1] = EOS;
			for (i=0; uttline[i] == '\n'; i++) ;
			if (uttline[i] == ' ' && isIndBlankHeaders)
				fprintf(fpout, "@Indent\n");
			for (; uttline[i] == ' ' || uttline[i] == '\n'; i++) ;
			if (uttline[i]) {
				uS.remblanks(uttline+i);
				if (isLowerCase)
					lowercaseTier(uttline+i);
				if (uttline[i] == '@') {
					addTab(uttline+i);
					fprintf(fpout, "%s\n", uttline+i);
				} else {
					if (!isHeritage)
						addUttDel(uttline+i);
					printout("*TXT:", uttline+i, NULL, NULL, TRUE);
				}
			}
			*uttline = EOS;
			qf = FALSE;
			i = 0;
			uttline[i++] = utterance->line[pos];
			pos++;
		} else if (utterance->line[pos] == '\n' && isLineUtt) {
			uttline[i] = EOS;
			for (i=0; uttline[i] == '\n'; i++) ;
			if (uttline[i] == ' ' && isIndBlankHeaders)
				fprintf(fpout, "@Indent\n");
			for (; uttline[i] == ' ' || uttline[i] == '\n'; i++) ;
			if (uttline[i]) {
				uS.remblanks(uttline+i);
				if (isLowerCase)
					lowercaseTier(uttline+i);
				if (uttline[i] == '@') {
					addTab(uttline+i);
					fprintf(fpout, "%s\n", uttline+i);
				} else {
					if (!isHeritage)
						addUttDel(uttline+i);
					printout("*TXT:", uttline+i, NULL, NULL, TRUE);
				}
			}
			*uttline = EOS;
			qf = FALSE;
			i = 0;
			pos++;
			while (utterance->line[pos] == ' ' || utterance->line[pos] == '\t')
				pos++;
		} else if (utterance->line[pos] == '.' || utterance->line[pos] == '!' || utterance->line[pos] == '?') {
		   uttline[i] = EOS;
		   for (i=0; uttline[i] == '\n'; i++) ;
		   if (uttline[i] == ' ' && isIndBlankHeaders)
			   fprintf(fpout, "@Indent\n");
		   for (; uttline[i] == ' ' || uttline[i] == '\n'; i++) ;
		   if (uttline[i]) {
			   uS.remblanks(uttline+i);
			   if (isLowerCase)
				   lowercaseTier(uttline+i);
			   printout("*TXT:", uttline+i, NULL, NULL, TRUE);
		   }
		   *uttline = EOS;
		   qf = FALSE;
		   i = 0;
		   pos++;
		   while (utterance->line[pos] == ' ' || utterance->line[pos] == '\t')
			   pos++;
	   } else if (i > UTTLINELEN-80 && (utterance->line[pos] == ' ' || utterance->line[pos] == '\t')) {
		  uttline[i] = EOS;
		  for (i=0; uttline[i] == '\n'; i++) ;
		  if (uttline[i] == ' ' && isIndBlankHeaders)
			  fprintf(fpout, "@Indent\n");
		  for (; uttline[i] == ' ' || uttline[i] == '\n'; i++) ;
		  if (uttline[i]) {
			  uS.remblanks(uttline+i);
			  if (isLowerCase)
				  lowercaseTier(uttline+i);
			  printout("*TXT:", uttline+i, NULL, NULL, TRUE);
		  }
		  *uttline = EOS;
		  qf = FALSE;
		  i = 0;
		  pos++;
		  while (utterance->line[pos] == ' ' || utterance->line[pos] == '\t')
			  pos++;
		} else
			pos++;
		if (utterance->line[pos] == EOS) {
			if (feof(fpin))
				break;
			if (fgets_cr(utterance->line, UTTLINELEN, fpin) == NULL)
				break;
			pos = 0L;
		}
	} while (TRUE) ;
	if (*uttline != EOS) {
		uttline[i] = EOS;
		for (i=0; uttline[i] == '\n'; i++) ;
		if (uttline[i] == ' ' && isIndBlankHeaders)
			fprintf(fpout, "@Indent\n");
		for (; uttline[i] == ' ' || uttline[i] == '\n'; i++) ;
		if (uttline[i]) {
			uS.remblanks(uttline+i);
			if (isLowerCase)
				lowercaseTier(uttline+i);
			if (uttline[i] == '@' && isLineUtt) {
				uS.remblanks(uttline+i);
				addTab(uttline+i);
				fprintf(fpout, "%s\n", uttline+i);
			} else {
				if (!isHeritage)
					addUttDel(uttline+i);
				printout("*TXT:", uttline+i, NULL, NULL, TRUE);
			}
		}
	}
	fprintf(fpout, "@End\n");
}
