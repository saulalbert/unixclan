/**********************************************************************
	"Copyright 1990-2018 Brian MacWhinney. Use is subject to Gnu Public License
	as stated in the attached "gpl.txt" file."
*/


#define CHAT_MODE 1

#include "cu.h"
#include "c_curses.h"

#ifdef UNX
	#define RGBColor int
#endif

#if !defined(UNX)
#define _main flucalc_main
#define call flucalc_call
#define getflag flucalc_getflag
#define init flucalc_init
#define usage flucalc_usage
#endif

#define IS_WIN_MODE FALSE
#include "mul.h" 

extern char OverWriteFile;
extern char outputOnlyData;
extern char isLanguageExplicit;
extern char linkMain2Mor;
extern struct tier *defheadtier;
extern struct tier *headtier;

struct flucalc_speakers {
	char isSpeakerFound;
	char isMORFound;
	char isPSDFound; // +/.

	char *fname;
	char *sp;
	char *ID;

	float tm;
	float morUtt;
	float morWords;
	float morSyllables;

	float prolongation;	// :
	float broken_word;	// ^
	float block;		// ≠
	float PWR;			// ↫ ↫ pairs//Part Word Repetition; Sequences
	float PWRRU;		// ↫ - - ↫  // iterations
	float phono_frag;	// &+
	float WWR;			// how many times "the [/] the [/] the" occurs. In this case it is 1.
	float mWWR;			// how many times monosyllable "the [/] the [/] the" occurs. In this case it is 1.
	float WWRRU;		// [/]		// Whole Word repetition iterations
	float mWWRRU;		// [/]		// Whole monosyllable Word repetition iterations
	float phrase_repets;// <> [/]
	float word_revis;	// [//]
	float phrase_revis;	// <> [//]
	float pauses_cnt;	// (.)
	float pauses_dur;	// (2.4)
	float filled_pauses;// &-

	struct flucalc_speakers *next_sp;
} ;

struct flucalc_tnode {
	char *word;
	int count;
	struct flucalc_tnode *left;
	struct flucalc_tnode *right;
};

static char flucalc_ftime, isSyllWordsList, isWordMode;
static struct flucalc_speakers *sp_head;
static struct flucalc_tnode *rootWords;
static FILE *SyllWordsListFP;


void usage() {
	puts("FLUCALC creates a spreedsheet with a series of fluency measures.");
#ifdef UNX
	printf("FluCalc REQUIRES THE PRESENCE OF THE \"%%mor:\" TIER BY DEFAULT.\n");
	printf("TO USE ONLY THE MAIN SPEAKER TIER PLEASE USE \"-t%%mor:\" OPTION.\n");
#else
	printf("%c%cFluCalc REQUIRES THE PRESENCE OF THE \"%%mor:\" TIER BY DEFAULT.%c%c\n", ATTMARKER, error_start, ATTMARKER, error_end);
	printf("%c%cTO USE ONLY THE MAIN SPEAKER TIER PLEASE USE \"-t%%mor:\" OPTION.%c%c\n", ATTMARKER, error_start, ATTMARKER, error_end);
#endif
	printf("Usage: flucalc [%s] filename(s)\n",mainflgs());
	puts("+b : select word mode analyses (default: syllables mode)");
	puts("+e1: create file with words their syllables count");
	mainusage(FALSE);
#ifdef UNX
	printf("FluCalc REQUIRES THE PRESENCE OF THE \"%%mor:\" TIER BY DEFAULT.\n");
	printf("TO USE ONLY THE MAIN SPEAKER TIER PLEASE USE \"-t%%mor:\" OPTION.\n");
#else
	printf("%c%cFluCalc REQUIRES THE PRESENCE OF THE \"%%mor:\" TIER BY DEFAULT.%c%c\n", ATTMARKER, error_start, ATTMARKER, error_end);
	printf("%c%cTO USE ONLY THE MAIN SPEAKER TIER PLEASE USE \"-t%%mor:\" OPTION.%c%c\n", ATTMARKER, error_start, ATTMARKER, error_end);
#endif
	cutt_exit(0);
}

static struct flucalc_speakers *freespeakers(struct flucalc_speakers *p) {
	struct flucalc_speakers *ts;

	while (p) {
		ts = p;
		p = p->next_sp;
		if (ts->fname != NULL)
			free(ts->fname);
		if (ts->sp != NULL)
			free(ts->sp);
		if (ts->ID != NULL)
			free(ts->ID);
		free(ts);
	}
	return(NULL);
}

static void flucalc_freetree(struct flucalc_tnode *p) {
	if (p != NULL) {
		flucalc_freetree(p->left);
		flucalc_freetree(p->right);
		free(p->word);
		free(p);
	}
}

static void flucalc_error(char IsOutOfMem) {
	if (IsOutOfMem)
		fputs("ERROR: Out of memory.\n",stderr);
	sp_head = freespeakers(sp_head);
	if (SyllWordsListFP != NULL)
		fclose(SyllWordsListFP);
	flucalc_freetree(rootWords);
	cutt_exit(0);
}

static void flucalc_initTSVars(struct flucalc_speakers *ts) {
	ts->isMORFound = FALSE;
	ts->isPSDFound = FALSE;

	ts->tm		  = 0.0;
	ts->morUtt	  = 0.0;
	ts->morWords  = 0.0;
	ts->morSyllables= 0.0;

	ts->prolongation	= 0.0;
	ts->broken_word		= 0.0;
	ts->block			= 0.0;
	ts->PWR				= 0.0;
	ts->PWRRU			= 0.0;
	ts->phono_frag		= 0.0;
	ts->WWR				= 0.0;
	ts->mWWR			= 0.0;
	ts->WWRRU			= 0.0;
	ts->mWWRRU			= 0.0;
	ts->phrase_repets	= 0.0;
	ts->word_revis	    = 0.0;
	ts->phrase_revis    = 0.0;
	ts->pauses_cnt		= 0.0;
	ts->pauses_dur		= 0.0;
	ts->filled_pauses   = 0.0;
}

static struct flucalc_speakers *flucalc_FindSpeaker(char *fname, char *sp, char *ID, char isSpeakerFound) {
	struct flucalc_speakers *ts, *tsp;

	uS.remblanks(sp);
	for (ts=sp_head; ts != NULL; ts=ts->next_sp) {
		if (uS.mStricmp(ts->fname, fname) == 0) {
			if (uS.partcmp(ts->sp, sp, FALSE, FALSE)) {
				ts->isSpeakerFound = isSpeakerFound;
				return(ts);
			}
		}
	}
	if ((ts=NEW(struct flucalc_speakers)) == NULL)
		flucalc_error(TRUE);
	if ((ts->fname=(char *)malloc(strlen(fname)+1)) == NULL) {
		free(ts);
		flucalc_error(TRUE);
	}
	if (sp_head == NULL) {
		sp_head = ts;
	} else {
		for (tsp=sp_head; tsp->next_sp != NULL; tsp=tsp->next_sp) ;
		tsp->next_sp = ts;
	}
	ts->next_sp = NULL;
	strcpy(ts->fname, fname);
	if ((ts->sp=(char *)malloc(strlen(sp)+1)) == NULL) {
		flucalc_error(TRUE);
	}
	strcpy(ts->sp, sp);
	if (ID == NULL)
		ts->ID = NULL;
	else {
		if ((ts->ID=(char *)malloc(strlen(ID)+1)) == NULL)
			flucalc_error(TRUE);
		strcpy(ts->ID, ID);
	}
	ts->isSpeakerFound = isSpeakerFound;
	flucalc_initTSVars(ts);
	return(ts);
}

void init(char f) {
	int i;
	FNType debugfile[FNSize];
	struct tier *nt;

	if (f) {
		sp_head = NULL;
		flucalc_ftime = TRUE;
		if (defheadtier != NULL) {
			if (defheadtier->nexttier != NULL)
				free(defheadtier->nexttier);
			free(defheadtier);
			defheadtier = NULL;
		}
		mor_initwords();
		outputOnlyData = TRUE;
		OverWriteFile = TRUE;
		combinput = TRUE;
		isSyllWordsList = FALSE;
		SyllWordsListFP = NULL;
		rootWords = NULL;
		isWordMode = FALSE;
	} else {
		if (flucalc_ftime) {
			flucalc_ftime = FALSE;
			if (chatmode) {
				maketierchoice("@ID:",'+',FALSE);
				maketierchoice("%mor:",'+',FALSE);
			} else {
				fprintf(stderr, "FluCalc can only run on CHAT data files\n\n");
				cutt_exit(0);
			}
			i = 0;
			for (nt=headtier; nt != NULL; nt=nt->nexttier) {
				if (nt->tcode[0] == '*') {
					i++;
				}
			}
			if (i != 1) {
				fprintf(stderr, "\nPlease specify only one speaker tier code with \"+t\" option.\n");
				cutt_exit(0);
			}
			for (i=0; GlobalPunctuation[i]; ) {
				if (GlobalPunctuation[i] == '!' ||
					GlobalPunctuation[i] == '?' ||
					GlobalPunctuation[i] == '.') 
					strcpy(GlobalPunctuation+i,GlobalPunctuation+i+1);
				else
					i++;
			}
			if (!f_override)
				stout = FALSE;
			AddCEXExtension = ".xls";
			linkMain2Mor = TRUE;
#if !defined(CLAN_SRV)
			if (isSyllWordsList) {
				strcpy(debugfile, "word_syllables.cex");
				SyllWordsListFP = fopen(debugfile, "w");
				if (SyllWordsListFP == NULL) {
					fprintf(stderr, "Can't create file \"%s\", perhaps it is opened by another application\n", debugfile);
				}
#ifdef _MAC_CODE
				else
					settyp(debugfile, 'TEXT', the_file_creator.out, FALSE);
#endif
			}
#endif // !defined(CLAN_SRV)
		}
	}
}

void getflag(char *f, char *f1, int *i) {
	f++;
	switch(*f++) {
		case 'b':
			no_arg_option(f);
			OverWriteFile = FALSE;
			isWordMode = TRUE;
			break;
		case 'e':
			if (*f == '1')
				isSyllWordsList = TRUE;
			else {
				fprintf(stderr,"Invalid argument for option: %s\n", f-2);
				cutt_exit(0);
			}
			break;
		case 's':
			if (*f == '[' && *(f+1) == '-') {
				if (*(f-2) == '+')
					isLanguageExplicit = 2;
				maingetflag(f-2,f1,i);
			} else if ((*f == '[' && *(f+1) == '+') || ((*f == '+' || *f == '~') && *(f+1) == '[' && *(f+2) == '+')) {
				maingetflag(f-2,f1,i);
			} else {
				fprintf(stderr, "Please specify only postcodes, \"[+ ...]\", or precodes \"[- ...]\" with +/-s option.");
				cutt_exit(0);
			}
			break;
		case 't':
			if (*(f-2) == '+' && *f == '*') {
				maingetflag(f-2,f1,i);
			} else {
				fprintf(stderr, "\nPlease specify only one speaker tier code with \"+t\" option.\n");
				cutt_exit(0);
			}
			break;
		default:
			maingetflag(f-2,f1,i);
			break;
	}
}

static void flucalc_pr_result(void) {
	char  *sFName;
	float SLD, TD, X, devNum;
	struct flucalc_speakers *ts;

	if (sp_head == NULL) {
		fprintf(stderr, "\nERROR: No speaker matching +t option found\n\n");
	}
	excelHeader(fpout, newfname, 95);
	excelRow(fpout, ExcelRowStart);
	excelStrCell(fpout, "File");
	excelCommasStrCell(fpout, "Language,Corpus,Code,Age(Month),Sex,Group,SES,Role,Education,Custom_field");

	excelStrCell(fpout, "mor Utts");
	excelStrCell(fpout, "mor Words");
	excelStrCell(fpout, "mor syllables");
	excelStrCell(fpout, "words/min");
	excelStrCell(fpout, "syllables/min");

	excelStrCell(fpout, "# Prolongation");
	excelStrCell(fpout, "% Prolongation");
	excelStrCell(fpout, "# Broken word");
	excelStrCell(fpout, "% Broken word");
	excelStrCell(fpout, "# Block");
	excelStrCell(fpout, "% Block");
	excelStrCell(fpout, "# PWR");
	excelStrCell(fpout, "% PWR");
	excelStrCell(fpout, "# PWR-RU");
	excelStrCell(fpout, "% PWR-RU");
	excelStrCell(fpout, "# WWR");
	excelStrCell(fpout, "% WWR");
	excelStrCell(fpout, "# mono-WWR");
	excelStrCell(fpout, "% mono-WWR");
	excelStrCell(fpout, "# WWR-RU");
	excelStrCell(fpout, "% WWR-RU");
	excelStrCell(fpout, "# mono-WWR-RU");
	excelStrCell(fpout, "% mono-WWR-RU");
	excelStrCell(fpout, "Mean RU");
	excelStrCell(fpout, "# Phonological fragment");
	excelStrCell(fpout, "% Phonological fragment");
	excelStrCell(fpout, "# Phrase repetitions");
	excelStrCell(fpout, "% Phrase repetitions");
	excelStrCell(fpout, "# Word revisions");
	excelStrCell(fpout, "% Word revisions");
	excelStrCell(fpout, "# Phrase revisions");
	excelStrCell(fpout, "% Phrase revisions");
	excelStrCell(fpout, "# Pauses");
	excelStrCell(fpout, "% Pauses");
//	excelStrCell(fpout, "# Pause duration");
//	excelStrCell(fpout, "% Pause duration");
	excelStrCell(fpout, "# Filled pauses");
	excelStrCell(fpout, "% Filled pauses");

	excelStrCell(fpout, "# TD");
	excelStrCell(fpout, "% TD");
	excelStrCell(fpout, "# SLD");
	excelStrCell(fpout, "% SLD");

	excelStrCell(fpout, "# Total (SLD+TD)");
	excelStrCell(fpout, "% Total (SLD+TD)");

	excelStrCell(fpout, "SLD Ratio");

	excelStrCell(fpout, "Weighted SLD");
	excelRow(fpout, ExcelRowEnd);

	for (ts=sp_head; ts != NULL; ts=ts->next_sp) {
		if (!ts->isSpeakerFound) {
			fprintf(stderr, "\nWARNING: No data found for speaker \"%s\" in file \"%s\"\n\n", ts->sp, ts->fname);
			continue;
		}
// ".xls"
		sFName = strrchr(ts->fname, PATHDELIMCHR);
		if (sFName != NULL)
			sFName = sFName + 1;
		else
			sFName = ts->fname;
		excelRow(fpout, ExcelRowStart);
		excelStrCell(fpout, sFName);
		if (ts->ID) {
			excelOutputID(fpout, ts->ID);
		} else {
			excelCommasStrCell(fpout, ".,.");
			excelCommasStrCell(fpout, ts->sp);
			excelCommasStrCell(fpout, ".,.,.,.,.,.,.");
		}
		if (!ts->isMORFound || ts->morWords == 0.0 || ts->morSyllables == 0.0) {
			fprintf(stderr, "\n*** File \"%s\": Speaker \"%s\"\n", sFName, ts->sp);
			fprintf(stderr, "WARNING: Speaker \"%s\" has no \"%s\" tiers.\n\n", ts->sp, "%mor:");
			excelNumCell(fpout, "%.0f", ts->morUtt);
			excelNumCell(fpout, "%.0f", ts->morWords);
			excelNumCell(fpout, "%.0f", ts->morSyllables);
			excelCommasStrCell(fpout, "N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A");
		} else {
			excelNumCell(fpout, "%.0f", ts->morUtt);
			excelNumCell(fpout, "%.0f", ts->morWords);
			excelNumCell(fpout, "%.0f", ts->morSyllables);

			if (ts->tm == 0.0)
				excelStrCell(fpout, "N/A");
			else
				excelNumCell(fpout, "%.3f", ts->morWords / (ts->tm / 60.0000));

			if (ts->tm == 0.0)
				excelStrCell(fpout, "N/A");
			else
				excelNumCell(fpout, "%.3f", ts->morSyllables / (ts->tm / 60.0000));

			if (isWordMode)
				devNum = ts->morWords;
			else
				devNum = ts->morSyllables;

			excelNumCell(fpout, "%.3f", ts->prolongation);
			excelNumCell(fpout, "%.3f", (ts->prolongation/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->broken_word);
			excelNumCell(fpout, "%.3f", (ts->broken_word/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->block);
			excelNumCell(fpout, "%.3f", (ts->block/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->PWR);
			excelNumCell(fpout, "%.3f", (ts->PWR/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->PWRRU);
			excelNumCell(fpout, "%.3f", (ts->PWRRU/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->WWR);
			excelNumCell(fpout, "%.3f", (ts->WWR/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->mWWR);
			excelNumCell(fpout, "%.3f", (ts->mWWR/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->WWRRU);
			excelNumCell(fpout, "%.3f", (ts->WWRRU/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->mWWRRU);
			excelNumCell(fpout, "%.3f", (ts->mWWRRU/devNum)*100.0000);
			X = ts->PWR + ts->WWR;
			if (X <= 0.0)
				X = 0.0;
			else {
				X = (ts->PWRRU + ts->WWRRU) / X;
			}
			excelNumCell(fpout, "%.3f", X);
			excelNumCell(fpout, "%.3f", ts->phono_frag);
			excelNumCell(fpout, "%.3f", (ts->phono_frag/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->phrase_repets);
			excelNumCell(fpout, "%.3f", (ts->phrase_repets/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->word_revis);
			excelNumCell(fpout, "%.3f", (ts->word_revis/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->phrase_revis);
			excelNumCell(fpout, "%.3f", (ts->phrase_revis/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->pauses_cnt);
			excelNumCell(fpout, "%.3f", (ts->pauses_cnt/devNum)*100.0000);
//			excelNumCell(fpout, "%.3f", ts->pauses_dur);
//			excelNumCell(fpout, "%.3f", (ts->pauses_dur/devNum)*100.0000);
			excelNumCell(fpout, "%.3f", ts->filled_pauses);
			excelNumCell(fpout, "%.3f", (ts->filled_pauses/devNum)*100.0000);

			TD = ts->phrase_repets+ts->word_revis+ts->phrase_revis+ts->pauses_cnt+ts->pauses_dur+ts->filled_pauses;
			excelNumCell(fpout, "%.3f", TD);
			excelNumCell(fpout, "%.3f", (TD/devNum)*100.0000);

			SLD = ts->prolongation+ts->broken_word+ts->block+ts->PWR+ts->phono_frag+ts->mWWR;
			excelNumCell(fpout, "%.3f", SLD);
			excelNumCell(fpout, "%.3f", (SLD/devNum)*100.0000);

			excelNumCell(fpout, "%.3f", SLD+TD);
			excelNumCell(fpout, "%.3f", ((SLD/devNum)*100.0000)+((TD/devNum)*100.0000));

			if ((SLD+TD) == 0.0)
				excelStrCell(fpout, "N/A");
			else
				excelNumCell(fpout, "%.3f", SLD / (SLD+TD));

			X = (ts->PWR * 100.0000 / ts->morSyllables) + (ts->mWWR * 100.0000 / ts->morSyllables);
			if (X > 0) {
				SLD = (X * ( ( (ts->PWRRU * 100.0000 / ts->morSyllables) + (ts->mWWRRU * 100.0000 / ts->morSyllables) ) / X) ) +
					  (2 * ( (ts->prolongation * 100.0000 / ts->morSyllables) + (ts->block * 100.0000 / ts->morSyllables) ) );
			} else {
				SLD = (2 * ( (ts->prolongation * 100.0000 / ts->morSyllables) + (ts->block * 100.0000 / ts->morSyllables) ) );
			}
			excelNumCell(fpout, "%.3f", SLD);
		}
		excelRow(fpout, ExcelRowEnd);
	}
	excelRow(fpout, ExcelRowEmpty);
	excelRow(fpout, ExcelRowEmpty);
	excelRowOneStrCell(fpout, ExcelBlkCell, "Interpretation of weighted SLD score: Scores above 4.0 is highly suggestive of clinical diagnosis of stuttering in young children");
	excelRowOneStrCell(fpout, ExcelBlkCell, "Weighted score formula (from Ambrose & Yairi, 1999): ((PWR + mono-WWR) * ((PWR-RU + mono-WWR-RU)/(PWR + mono- WWR))) + (2 * (prologations + blocks))");
	if (isWordMode)
		excelRowOneStrCell(fpout, ExcelRedCell, "This spreadsheet was run in word mode");
	else
		excelRowOneStrCell(fpout, ExcelRedCell, "This spreadsheet was run in syllable mode");
	excelFooter(fpout);
	sp_head = freespeakers(sp_head);
}

/*

 "Mean RUs" = (PWRRU + WWRRU) / (PWR + WWR)


 TD = phrase_repets+word_revis+phrase_revis+pauses_cnt+pauses_dur+filled_pauses;
 "#TD" = TD
 "%TD" = (TD/morWords)*100.0000


 SLD = prolongation+broken_word+block+PWR+phono_frag+WWR;
 "#SLD" = SLD
 "%SLD" = (SLD/morWords)*100.0000


 "#Total (SLD+TD)" = SLD + TD
 "%Total (SLD+TD)" = ((SLD/morWords)*100.0000) + ((TD/morWords)*100.0000)


 "SLD Ratio" = SLD / (SLD + TD)


 X = (PWR * 100.0000 / morSyllables) + (mWWR * 100.0000 / morSyllables)
 SLD = (X * ( ( (PWRRU * 100.0000 / morSyllables) + (mWWRRU * 100.0000 / morSyllables) ) / X) ) +
       (2 * ( (prolongation * 100.0000 / morSyllables) + (block * 100.0000 / morSyllables) ) )


 X = ( PWR + mWWR )
 If X is not equal to zero, then
	SLD = ( X * ( ( PWRRU + mWWRRU) / X  ) ) + ( 2 * ( prolongation + block ) )
 If X is equal to zero, then
	SLD = ( 2 * ( prolongation + block ) )


 SLD = ( (PWR + mWWR) * ( (PWRRU + mWWRRU) / (PWR + mWWR) ) ) + (2 * ( prolongation + block ) )
 "Weighted SLD" = SLD);
*/

static int countSyllables(char *word) {
	int i, j, startIndex, vCnt;
	char tWord[BUFSIZ+1], isRepSeg, isBullet;

	if (word[0] == '[' || word[0] == '(' || word[0] == EOS)
		return(0);
	isRepSeg = FALSE;
	isBullet = FALSE;
	j = 0;
	for (i=0; word[i] != EOS; i++) {
		if (UTF8_IS_LEAD((unsigned char)word[i]) && word[i] == (char)0xE2) {
			if (word[i+1] == (char)0x86 && word[i+2] == (char)0xAB) {
				isRepSeg = !isRepSeg;
			}
		} else if (word[i] == HIDEN_C)
			isBullet = !isBullet;
		if (!isRepSeg && !isBullet && isalnum(word[i]))
			tWord[j++] = word[i];
	}
	tWord[j] = EOS;
	if (tWord[0] == EOS)
		return(0);
	//	if (uS.mStricmp(tWord, "something") == 0)
	//		return(2);
	vCnt = 0;
	if (uS.mStrnicmp(tWord,"ice", 3) == 0) {
		vCnt = 1;
		startIndex = 3;
	} else if (uS.mStrnicmp(tWord,"some", 4) == 0 || uS.mStrnicmp(tWord,"fire", 4) == 0 ||
			   uS.mStrnicmp(tWord,"bake", 4) == 0 || uS.mStrnicmp(tWord,"base", 4) == 0 ||
			   uS.mStrnicmp(tWord,"bone", 4) == 0 || uS.mStrnicmp(tWord,"cake", 4) == 0 ||
			   uS.mStrnicmp(tWord,"care", 4) == 0 || uS.mStrnicmp(tWord,"dare", 4) == 0 ||
			   uS.mStrnicmp(tWord,"fire", 4) == 0 || uS.mStrnicmp(tWord,"fuse", 4) == 0 ||
			   uS.mStrnicmp(tWord,"game", 4) == 0 || uS.mStrnicmp(tWord,"home", 4) == 0 ||
			   uS.mStrnicmp(tWord,"juke", 4) == 0 || uS.mStrnicmp(tWord,"lake", 4) == 0 ||
			   uS.mStrnicmp(tWord,"life", 4) == 0 || uS.mStrnicmp(tWord,"mine", 4) == 0 ||
			   uS.mStrnicmp(tWord,"mole", 4) == 0 || uS.mStrnicmp(tWord,"name", 4) == 0 ||
			   uS.mStrnicmp(tWord,"nose", 4) == 0 || uS.mStrnicmp(tWord,"note", 4) == 0 ||
			   uS.mStrnicmp(tWord,"race", 4) == 0 || uS.mStrnicmp(tWord,"rice", 4) == 0 ||
			   uS.mStrnicmp(tWord,"side", 4) == 0 || uS.mStrnicmp(tWord,"take", 4) == 0 ||
			   uS.mStrnicmp(tWord,"tape", 4) == 0 || uS.mStrnicmp(tWord,"time", 4) == 0 ||
			   uS.mStrnicmp(tWord,"wine", 4) == 0 || uS.mStrnicmp(tWord,"wipe", 4) == 0) {
		vCnt = 1;
		startIndex = 4;
	} else if (uS.mStrnicmp(tWord,"goose", 5) == 0 || uS.mStrnicmp(tWord,"grade", 5) == 0 ||
			   uS.mStrnicmp(tWord,"grape", 5) == 0 || uS.mStrnicmp(tWord,"horse", 5) == 0 ||
			   uS.mStrnicmp(tWord,"house", 5) == 0 || uS.mStrnicmp(tWord,"phone", 5) == 0 ||
			   uS.mStrnicmp(tWord,"snake", 5) == 0 || uS.mStrnicmp(tWord,"space", 5) == 0 ||
			   uS.mStrnicmp(tWord,"store", 5) == 0 || uS.mStrnicmp(tWord,"stove", 5) == 0 ||
			   uS.mStrnicmp(tWord,"voice", 5) == 0 || uS.mStrnicmp(tWord,"waste", 5) == 0) {
		vCnt = 1;
		startIndex = 5;
	} else if (uS.mStrnicmp(tWord,"cheese", 6) == 0) {
		vCnt = 1;
		startIndex = 6;
	} else if (uS.mStrnicmp(tWord,"police", 6) == 0) {
		vCnt = 2;
		startIndex = 6;
	} else {
		vCnt = 0;
		startIndex = 0;
	}
	for (i=startIndex; tWord[i] != EOS; i++) {
		if ((i == startIndex || !uS.isVowel(tWord+i-1)) && uS.isVowel(tWord+i)) {
			if ((tWord[i] == 'e' || tWord[i] == 'E') && tWord[i+1] == EOS) {
				if (vCnt == 0)
					vCnt++;
				else if (i > 0 && (tWord[i-1] == 'l' || tWord[i-1] == 'L'))
					vCnt++;
			} else
				vCnt++;
		}
	}
	return(vCnt);
}

static struct flucalc_tnode *flucalc_tree(struct flucalc_tnode *p, char *word, int count) {
	int cond;
	struct flucalc_tnode *t = p;

	if (p == NULL) {
		if ((p=NEW(struct flucalc_tnode)) == NULL)
			flucalc_error(TRUE);
		p->word = (char *)malloc(strlen(word)+1);
		if (p->word == NULL)
			flucalc_error(TRUE);
		strcpy(p->word, word);
		p->count = count;
		p->left = p->right = NULL;
	} else if ((cond=strcmp(word,p->word)) < 0)
		p->left = flucalc_tree(p->left, word, count);
	else if (cond > 0){
		for (; (cond=strcmp(word,p->word)) > 0 && p->right != NULL; p=p->right) ;
		if (cond < 0)
			p->left = flucalc_tree(p->left, word, count);
		else if (cond > 0)
			p->right = flucalc_tree(p->right, word, count); /* if cond > 0 */
		return(t);
	}
	return(p);
}

static char isUttDel(char *line, int pos) {
	if (line[pos] == '?' && line[pos+1] == '|')
		;
	else if (uS.IsUtteranceDel(line, pos)) {
		if (!uS.atUFound(line, pos, &dFnt, MBF))
			return(TRUE);
	}
	return(FALSE);
}

static char isOnlyOneWordPreCode(char *line, int wi) {
	int  i, wCnt;
	char word[BUFSIZ+1];

	findWholeScope(wi, templineC);
	uS.remFrontAndBackBlanks(templineC);
	if (templineC[0] == EOS)
		return(TRUE);
	wCnt = 0;
	i = 0;
	while ((i=getword(utterance->speaker, templineC, word, NULL, i))) {
		if (word[0] != '[')
			wCnt++;
	}
	if (wCnt <= 1)
		return(TRUE);
	else
		return(FALSE);
}

static char isMonoWordPreCode(char *line, int wi) {
	int  i, wCnt, syllCnt;
	char word[BUFSIZ+1];

	findWholeScope(wi, templineC);
	uS.remFrontAndBackBlanks(templineC);
	if (templineC[0] == EOS)
		return(TRUE);
	wCnt = 0;
	syllCnt = 0;
	i = 0;
	while ((i=getword(utterance->speaker, templineC, word, NULL, i))) {
		if (word[0] != '[') {
			wCnt++;
			syllCnt += countSyllables(word);
		}
	}
	if (wCnt <= 1) {
		if (syllCnt == 1)
			return(TRUE);
	}
	return(FALSE);
}

static char isPreviousItemWWR(char *line, int wi) {

	for (wi--; wi >= 0 && (isSpace(line[wi]) || line[wi] == '\n'); wi--) ;
	if (wi < 3)
		return(FALSE);
	if ((isSpace(line[wi-3]) || line[wi-3] == '\n') && line[wi-2] == '[' && line[wi-1] == '/' && line[wi] == ']')
		return(TRUE);
	return(FALSE);
}

static float roundFloat(double num) {
	long t;

	t = (long)num;
	num = num - t;
	if (num > 0.5)
		t++;
	return(t);
}

void call()	{		/* tabulate array of word lengths */
	int  i, j, wi, syllCnt;
	char word[BUFSIZ+1], *ws, *wm, *s;
	char isRepSeg, isWWRFound, ismWWRFound;
	long stime, etime;
	double tNum;
	struct flucalc_speakers *ts;
	MORFEATS word_feats, *clitic, *feat;

	ts = NULL;
	currentatt = 0;
	currentchar = (char)getc_cr(fpin, &currentatt);
	while (getwholeutter()) {
/*
 if (lineno > tlineno) {
	tlineno = lineno + 200;
 }
 */
		if (*utterance->speaker == '@') {
			if (uS.partcmp(utterance->speaker,"@ID:",FALSE,FALSE)) {
				if (isIDSpeakerSpecified(utterance->line, templineC, TRUE)) {
					uS.remblanks(utterance->line);
					flucalc_FindSpeaker(oldfname, templineC, utterance->line, FALSE);
				}
			}
		} else if (*utterance->speaker == '*') {
			strcpy(templineC, utterance->speaker);
			ts = flucalc_FindSpeaker(oldfname, templineC, NULL, TRUE);
			if (ts != NULL) {
				for (i=0; utterance->line[i] != EOS; i++) {

					if (utterance->line[i] == HIDEN_C && isdigit(utterance->line[i+1])) {
						if (getMediaTagInfo(utterance->line+i, &stime, &etime)) {
							tNum = etime;
							tNum = tNum - stime;
							tNum = tNum / 1000.0000;
							ts->tm = ts->tm + roundFloat(tNum);
						}
					}

					if ((i == 0 || uS.isskip(utterance->line,i-1,&dFnt,MBF)) && utterance->line[i] == '+' &&
						uS.isRightChar(utterance->line,i+1,',',&dFnt, MBF) && ts->isPSDFound) {
						if (ts->morUtt > 0.0)
							ts->morUtt--;
						ts->isPSDFound = FALSE;
					}
				}
				isWWRFound = 0;
				ismWWRFound = 0;
				i = 0;
				while ((i=getword(utterance->speaker, utterance->line, word, &wi, i))) {
					isRepSeg = FALSE;
					for (j=0; word[j] != EOS; j++) {
						if (word[j] == ':')
							ts->prolongation++;
						else if (word[j] == '^')
							ts->broken_word++;
						else if (UTF8_IS_LEAD((unsigned char)word[j]) && word[j] == (char)0xE2) {
							if (word[j+1] == (char)0x89 && word[j+2] == (char)0xA0) // ≠
								ts->block++;
							else if (word[j+1] == (char)0x86 && word[j+2] == (char)0xAB) { // ↫ - - ↫
								isRepSeg = !isRepSeg;
								if (isRepSeg) {
									ts->PWRRU++;
									ts->PWR++;
								}
							}
						} else if (word[j] == '&' && word[j+1] == '+') {
							ts->phono_frag++;
						} else if (word[j] == '&' && word[j+1] == '-') {
							ts->filled_pauses++;
						}
						if (isRepSeg && word[j] == '-') // ↫ - - ↫
							ts->PWRRU++;
					}
					if (!strcmp(word, "[/]")) {
						if (isOnlyOneWordPreCode(utterance->line, wi)) {
							if (isWWRFound == 0)
								isWWRFound = 1;
							ts->WWRRU++;
							if (isMonoWordPreCode(utterance->line, wi)) {
								if (ismWWRFound == 0)
									ismWWRFound = 1;
								ts->mWWRRU++;
							}
						} else
							ts->phrase_repets++;
					} else {
						if (!isPreviousItemWWR(utterance->line, wi)) {
							isWWRFound = 0;
							ismWWRFound = 0;
						}
						if (!strcmp(word, "[//]")) {
							if (isOnlyOneWordPreCode(utterance->line, wi))
								ts->word_revis++;
							else
								ts->phrase_revis++;
						} else if (word[0] == '(' && uS.isPause(word, 0, NULL, &j)) {
							ts->pauses_cnt++;
							ts->pauses_dur = ts->pauses_dur + getPauseTimeDuration(word);
						}
					}
					if (isWWRFound == 1) {
						ts->WWR++;
						isWWRFound = 2;
					}
					if (ismWWRFound == 1) {
						ts->mWWR++;
						ismWWRFound = 2;
					}
				}
			}
			strcpy(spareTier1, utterance->line);
		} else if (*utterance->speaker == '%' && ts != NULL) {
			ts->isMORFound = TRUE;

			i = 0;
			while ((i=getNextDepTierPair(uttline, word, templineC4, NULL, i)) != 0) {
				if (word[0] != EOS && templineC4[0] != EOS) {
					if (strchr(word, '|') != NULL) {
						wm = word;
						ws = templineC4;
					} else {
						wm = templineC4;
						ws = word;
					}
					syllCnt = 0;
					s = strchr(wm, '|');
					if (s != NULL) {
						ts->morWords++;
						if (*(s+1) == '+') {
							strcpy(templineC2, wm);
							if (ParseWordMorElems(templineC2, &word_feats) == FALSE)
								flucalc_error(FALSE);
							for (clitic=&word_feats; clitic != NULL; clitic=clitic->clitc) {
								if (clitic->compd != NULL) {
									for (feat=clitic; feat != NULL; feat=feat->compd) {
										if (feat->stem != NULL && feat->stem[0] != EOS) {
											syllCnt += countSyllables(feat->stem);
											syllCnt += countSyllables(ws+strlen(feat->stem));
											break;
										}
									}
									break;
								}
/*
								else {
									if (clitic->stem != NULL && clitic->stem[0] != EOS) {
										syllCnt += countSyllables(clitic->stem);
									}
								}
*/
							}
							freeUpFeats(&word_feats);
						}
						if (syllCnt == 0) {
							syllCnt += countSyllables(ws);
						}
						if (SyllWordsListFP != NULL) {
							rootWords = flucalc_tree(rootWords, ws, syllCnt);
						}
						ts->morSyllables += (float)syllCnt;
					} else {
						if (isTierContSymbol(wm, 0, FALSE))  //    +.  +/.  +/?  +//?  +...  +/.?   ===>   +,
							ts->isPSDFound = TRUE;
						for (j=0; wm[j] != EOS; j++) {
							if (isUttDel(wm, j)) {
								ts->morUtt = ts->morUtt + (float)1.0;
								break;
							}
						}
					}
				}
			}
/*
			i = 0;
			while ((i=getword(utterance->speaker, uttline, word, NULL, i))) {
				if (strchr(word, '|') != NULL) {
					ts->morWords++;
// count Syllables beg
					strcpy(templineC2, word);
					if (ParseWordMorElems(templineC2, &word_feats) == FALSE)
						flucalc_error(FALSE);
					for (clitic=&word_feats; clitic != NULL; clitic=clitic->clitc) {
						if (clitic->compd != NULL) {
							for (feat=clitic; feat != NULL; feat=feat->compd) {
								if (feat->stem != NULL && feat->stem[0] != EOS) {
									ts->morSyllables += countSyllables(feat->stem);
								}
							}
						} else {
							if (clitic->stem != NULL && clitic->stem[0] != EOS) {
								ts->morSyllables += countSyllables(clitic->stem);
							}
						}
					}
					freeUpFeats(&word_feats);
// count Syllables end
				} else {
					if (isTierContSymbol(word, 0, FALSE))  //    +.  +/.  +/?  +//?  +...  +/.?   ===>   +,
						ts->isPSDFound = TRUE;
					for (j=0; word[j] != EOS; j++) {
						if (isUttDel(word, j)) {
							ts->morUtt = ts->morUtt + (float)1.0;
							break;
						}
					}
				}
			}
*/
		}
	}
}

static void flucalc_treeprint(struct flucalc_tnode *p) {
	if (p != NULL) {
		flucalc_treeprint(p->left);
		do {
			fprintf(SyllWordsListFP,"%s %d\n", p->word, p->count);
			if (p->right == NULL)
				break;
			if (p->right->left != NULL) {
				flucalc_treeprint(p->right);
				break;
			}
			p = p->right;
		} while (1);
	}
}

CLAN_MAIN_RETURN main(int argc, char *argv[]) {
	isWinMode = IS_WIN_MODE;
	CLAN_PROG_NUM = FLUCALC;
	chatmode = CHAT_MODE;
	OnlydataLimit = 1;
	UttlineEqUtterance = FALSE;
	rootWords = NULL;
	isSyllWordsList = FALSE;
	SyllWordsListFP = NULL;
	bmain(argc,argv,flucalc_pr_result);
	if (SyllWordsListFP != NULL) {
		flucalc_treeprint(rootWords);
		fclose(SyllWordsListFP);
	}
	flucalc_freetree(rootWords);
	sp_head = freespeakers(sp_head);
}
