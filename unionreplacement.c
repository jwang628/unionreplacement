/*-------------------------------------------------------------------------
 *  
 * unionreplacement is a PostgreSQL extension which replace OR with UNION 
 * 
 * This program is open source, licensed under the PostgreSQL license.
 * For license terms, see the LICENSE file.
 *          
 * Copyright (c) 2024 James Wang
 *            
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "access/xact.h"
#include "parser/parse_node.h"
#include "parser/analyze.h"
#include "parser/parser.h"
#include "tcop/tcopprot.h"
#include "tcop/utility.h"
#include "utils/guc.h"
#include "utils/snapmgr.h"
#include "utils/memutils.h"
#if PG_VERSION_NUM <= 90600
#include "storage/lwlock.h"
#endif
#include "pgstat.h"
#include "storage/ipc.h"
#include "storage/spin.h"
#include "miscadmin.h"
#if PG_VERSION_NUM >= 90600
#include "nodes/extensible.h"
#endif
#if PG_VERSION_NUM > 120000
#include "nodes/pathnodes.h"
#endif
#include "nodes/plannodes.h"
#include "utils/datum.h"
#include "utils/builtins.h"
#include "unistd.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "ur.h"

PG_MODULE_MAGIC;

/*
 * maximum number of rules processed
 * by the extension defined as GUC
 */
static  ParseState      *new_static_pstate = NULL;
static  Query           *new_static_query = NULL;  

/* Saved hook values in case of unload */
#if PG_VERSION_NUM >= 150000
static shmem_request_hook_type prev_shmem_request_hook = NULL;
#endif
static post_parse_analyze_hook_type prev_post_parse_analyze_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;
static ExecutorStart_hook_type prev_executor_start_hook = NULL;

static  void    ur_reanalyze(const char *new_query_string);
#if PG_VERSION_NUM < 140000
static  void    ur_analyze(ParseState *pstate, Query *query);
#else
static  void    ur_analyze(ParseState *pstate, Query *query, JumbleState *jstate);
#endif
/*
 * Global shared state:
 * SQL translation rules are stored in shared memory 
 */

/*---- Function declarations ----*/

void		_PG_init(void);
void		_PG_fini(void);

static  void 	ur_exec(QueryDesc *queryDesc, int eflags);

/*
 * Module load callback
 */
void
_PG_init(void)
{
	elog(NOTICE, "unionreplacement:_PG_init():entry"); //create extension would not call this

	if (!process_shared_preload_libraries_in_progress)
		return;

	prev_post_parse_analyze_hook = post_parse_analyze_hook;
	post_parse_analyze_hook = ur_analyze;
	prev_executor_start_hook = ExecutorStart_hook;
 	ExecutorStart_hook = ur_exec;	

	elog(NOTICE, "unionreplacement:_PG_init():exit");
}


/*
 *  Module unload callback
 */
void
_PG_fini(void)
{
	shmem_startup_hook = prev_shmem_startup_hook;	
	post_parse_analyze_hook = prev_post_parse_analyze_hook;
	ExecutorStart_hook = prev_executor_start_hook;
}

/*
 * ur_exec
 *
 */
static void ur_exec(QueryDesc *queryDesc, int eflags)
{
#if PG_VERSION_NUM > 100000 

	int stmt_loc;
	int stmt_len;
	const char *src;

	{

		src = queryDesc->sourceText;
		stmt_loc = queryDesc->plannedstmt->stmt_location;
		stmt_len = queryDesc->plannedstmt->stmt_len;

		elog(NOTICE, "unionreplacement: ur_exec: src=%s", src);
		elog(NOTICE, "unionreplacement: ur_exec: stmt_loc=%d", stmt_loc);
		elog(NOTICE, "unionreplacement: ur_exec: stmt_len=%d", stmt_len);

		/*
	 	 * set stmt_location to -1 to avoid assertion failure in pgss_store:
 		 * Assert(query_len <= strlen(query)
	 	 */
		queryDesc->plannedstmt->stmt_location = -1;
		stmt_loc = queryDesc->plannedstmt->stmt_location;
		elog(DEBUG1, "unionreplacement: ur_exec: stmt_loc=%d", stmt_loc);
	}
#endif
	elog(NOTICE, "unionreplacement: ur_exec: eflags=%d", eflags);
	/*
 	 * must always execute here whatever PG_VERSION_NUM
 	 */

	if (prev_executor_start_hook)
                (*prev_executor_start_hook)(queryDesc, eflags);
	else	standard_ExecutorStart(queryDesc, eflags);
}
static void ur_clone_Query(Query *source, Query *target)
{
        target->type = source->type;
        target->commandType = source->commandType;
        target->querySource = source->querySource;
        target->queryId = source->queryId;
        target->canSetTag = source->canSetTag;
        target->utilityStmt = source->utilityStmt;
        target->resultRelation = source->resultRelation;
        target->hasAggs = source->hasAggs;
        target->hasWindowFuncs = source->hasWindowFuncs;
#if PG_VERSION_NUM > 100000 
        target->hasTargetSRFs = source->hasTargetSRFs;
#endif
        target->hasSubLinks = source->hasSubLinks;
        target->hasDistinctOn = source->hasDistinctOn;
        target->hasRecursive = source->hasRecursive;
        target->hasModifyingCTE = source->hasModifyingCTE;
        target->hasForUpdate = source->hasForUpdate;
        target->hasRowSecurity = source->hasRowSecurity;
        target->cteList = source->cteList;
        target->rtable = source->rtable;
        target->jointree = source->jointree;
        target->targetList = source->targetList;
#if PG_VERSION_NUM > 100000 
        target->override = source->override;
#endif
        target->onConflict = source->onConflict;
        target->returningList = source->returningList;
        target->groupClause = source->groupClause;
        target->groupingSets = source->groupingSets;
        target->havingQual = source->havingQual;
        target->windowClause = source->windowClause;
        target->distinctClause= source->distinctClause;
        target->sortClause= source->sortClause;
        target->limitOffset= source->limitOffset;
        target->limitCount= source->limitCount;
#if PG_VERSION_NUM > 130000 
        target->limitOption = source->limitOption;
#endif
        target->rowMarks= source->rowMarks;
        target->setOperations= source->setOperations;
        target->constraintDeps= source->constraintDeps;
        target->withCheckOptions = source->withCheckOptions;
#if PG_VERSION_NUM > 140000 
        target->limitOption = source->limitOption;
#endif
#if PG_VERSION_NUM > 100000 
        target->stmt_location=source->stmt_location;
        target->stmt_len=source->stmt_len;
#endif

}

static void ur_clone_ParseState(ParseState *source, ParseState *target)
{
        target->parentParseState = source->parentParseState;
        target->p_sourcetext = source->p_sourcetext;
        target->p_rtable= source->p_rtable;
        target->p_joinexprs= source->p_joinexprs;
        target->p_joinlist= source->p_joinlist;
        target->p_namespace= source->p_namespace;
        target->p_lateral_active= source->p_lateral_active;
        target->p_ctenamespace= source->p_ctenamespace;
        target->p_future_ctes= source->p_future_ctes;
        target->p_parent_cte= source->p_parent_cte;
        target->p_target_relation= source->p_target_relation;
        target->p_is_insert= source->p_is_insert;
        target->p_windowdefs= source->p_windowdefs;
        target->p_expr_kind= source->p_expr_kind;
        target->p_next_resno= source->p_next_resno;
        target->p_multiassign_exprs= source->p_multiassign_exprs;
        target->p_locking_clause= source->p_locking_clause;
        target->p_locked_from_parent= source->p_locked_from_parent;
#if PG_VERSION_NUM > 100000 
        target->p_resolve_unknowns= source->p_resolve_unknowns;
        target->p_queryEnv= source->p_queryEnv;
#endif
        target->p_hasAggs = source->p_hasAggs;
        target->p_hasWindowFuncs = source->p_hasWindowFuncs;
#if PG_VERSION_NUM > 100000 
        target->p_hasTargetSRFs= source->p_hasTargetSRFs;
#endif
        target->p_hasSubLinks= source->p_hasSubLinks;
        target->p_hasModifyingCTE= source->p_hasModifyingCTE;
#if PG_VERSION_NUM > 100000 
        target->p_last_srf = source->p_last_srf;
#endif
        target->p_pre_columnref_hook = source->p_pre_columnref_hook;
        target->p_post_columnref_hook = source->p_post_columnref_hook;
        target->p_paramref_hook = source->p_paramref_hook;
        target->p_coerce_param_hook = source->p_coerce_param_hook;
        target->p_ref_hook_state = source->p_ref_hook_state;
}
static void ur_reanalyze(const char *new_query_string)
{

        /* 
         * >>> FROM parse_analyze in src/backend/parser/analyze.c <<< 
         */

        ParseState      *new_pstate = make_parsestate(NULL);
        Query           *new_query = (Query *)NULL;

#if PG_VERSION_NUM >= 100000
        RawStmt         *new_parsetree;
#else
        Node            *new_parsetree;
#endif
        List            *raw_parsetree_list;
        ListCell        *lc1; 

        elog(DEBUG1, "unionreplacement: ur_reanalyze: entry");
        new_pstate->p_sourcetext = new_query_string;

        /* 
         * missing data: 
         * 1. numParams 
         * 2. ParamTypes 
         * 3. queryEnv
         */
        

        new_parsetree =  NULL;
        raw_parsetree_list = pg_parse_query(new_query_string);  

        /*
         * we assume only one SQL statement
         */
        foreach(lc1, raw_parsetree_list)
        {
#if PG_VERSION_NUM >= 100000
                new_parsetree = lfirst_node(RawStmt, lc1);
#else
                new_parsetree = (Node *) lfirst(lc1);
#endif
        }

        new_query = transformTopLevelStmt(new_pstate, new_parsetree);   

        new_static_pstate = new_pstate;
        new_static_query = new_query;

        elog(DEBUG1, "unionreplacement: ur_reanalyze: exit");
}

#define MAX_QUERY_SIZE 32768 // 32M crashed server :( Adjust this according to your needs
#if PG_VERSION_NUM < 140000
static void ur_analyze(ParseState *pstate, Query *query)
#else
static void ur_analyze(ParseState *pstate, Query *query, JumbleState *js)
#endif
{
		char *hint;
        /*
        ** not possible to access parameters using pstate->p_ref_hook_state
        ** because no easy way to check FixedParamState vs VarParamState
        ** (p_ref_hook_state is generic pointer in both cases
        ** and p_param_ref_hook refer to a static function in parse_params.c).
        */

        /* pstate->p_sourcetext is the current query text */    
		//_reanalyze("select 11;");//hard coded here
        elog(WARNING,"unionreplacement: ur_analyze: query %s",								 			pstate->p_sourcetext);
		hint = extractUrhint(pstate->p_sourcetext); //elog(ERROR, in ur.c will ruin this and process will terminated here :(
		if (strlen(hint) > 0) {
			ur_reanalyze(workflow(pstate->p_sourcetext, hint));
    	    // clone data 
        	ur_clone_ParseState(new_static_pstate, pstate);
	        elog(WARNING,"unionreplacement: ur_analyze: hint %s rewrite=true pstate->p_source_text %s", hint, pstate->p_sourcetext);
    	    ur_clone_Query(new_static_query, query);
		} else {
			ur_reanalyze(pstate->p_sourcetext);
		}

        /* no "standard_analyze" to call 
         * according to parse_analyze in analyze.c 
         */
        if (prev_post_parse_analyze_hook)
        {
#if PG_VERSION_NUM < 140000
                prev_post_parse_analyze_hook(pstate, query);
#else
                prev_post_parse_analyze_hook(pstate, query, js);
#endif
         }

        elog(DEBUG1, "unionreplacement: ur_analyze: exit");
}


