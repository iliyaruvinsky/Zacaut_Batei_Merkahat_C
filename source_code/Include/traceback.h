// traceback.h: Provide traceback capability in the event of segmentation fault.


#ifndef TRACEBACK_H

#define TRACEBACK_H



#define	MAX_TRACE	50

static char *trace_text [MAX_TRACE];
static int	trace_int	[MAX_TRACE];
static int	trace_date	[MAX_TRACE];
static int	trace_time	[MAX_TRACE];

static int	trace_depth			= 0;
static int	trace_prev_depth	= 0;


int	TRACE_PUSH			(char *text_in, int num_in);
int	TRACE_POP			(void);
int	TRACE_SWAP			(char *text_in, int num_in);
int	TRACE_DUMP			(void);
int	TRACE_SAVE_DEPTH	(void);
int	TRACE_RESTORE_DEPTH	(void);


#ifdef	MAIN

int	TRACE_PUSH	(char *text_in, int num_in)
{
	if (trace_depth >= MAX_TRACE)
	{
		return (-1);
	}
	else
	{
		trace_text [trace_depth] = text_in;
		trace_int  [trace_depth] = num_in;
		trace_date [trace_depth] = GetDate ();
		trace_time [trace_depth] = GetTime ();

		trace_depth++;

		return (0);
	}
}



int	TRACE_POP	(void)
{
	if (trace_depth > 0)
	{
		trace_depth--;
		return (0);
	}
	else
	{
		return (-1);
	}
}



int TRACE_SAVE_DEPTH	(void)
{
	trace_prev_depth = trace_depth;
	return (0);
}



int TRACE_RESTORE_DEPTH	(void)
{
	trace_depth = trace_prev_depth;
	return (0);
}



int	TRACE_SWAP	(char *text_in, int num_in)
{
	if (trace_depth > 0)
	{
		trace_text [trace_depth - 1] = text_in;
		trace_int  [trace_depth - 1] = num_in;
		trace_date [trace_depth - 1] = GetDate ();
		trace_time [trace_depth - 1] = GetTime ();
		return (0);
	}
	else
	{
		return (-1);
	}
}



int	TRACE_DUMP	(void)
{
	FILE	*fp;
	char	FName	[60];
	int		i;


	sprintf (FName, "TraceBack_%d_log", getpid ());
	fp = open_other_file (FName, "a");

	if (fp != NULL)
	{
		fprintf (fp, "PID %d terminating, %d at %d.\n\n", (long)getpid (), GetDate (), GetTime ());
		for (i = 0; i < trace_depth; i++)
		{
			fprintf (fp,
					 "%02d: %s (%d) %d %d\n",
					 i + 1,
					 trace_text [i],
					 trace_int  [i],
					 trace_date [i],
					 trace_time [i]);
		}

		close_log_file (fp);
	}
}

#endif


#endif	// ifndef TRACEBACK_H
