/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2017 MonetDB B.V.
 */

/*
 * opt_prelude
 * M. Kersten
 * These definitions are handy to have around in the optimizer
 */
#include "monetdb_config.h"
#include "opt_prelude.h"
#include "optimizer_private.h"

str abortRef;
str affectedRowsRef;
str aggrRef;
str alarmRef;
str algebraRef;
str andRef;
str appendidxRef;
str appendRef;
str assertRef;
str alter_seqRef;
str alter_schemaRef;
str alter_viewRef;
str alter_tableRef;
str alter_constraintRef;
str alter_typeRef;
str alter_userRef;
str alter_roleRef;
str alter_userRef;
str alter_indexRef;
str alter_functionRef;
str alter_triggerRef;
str alter_add_tableRef;
str alter_del_tableRef;
str alter_set_tableRef;
str attachRef;
str avgRef;
str batRef;
str batalgebraRef;
str arrayRef;
str basketRef;
str batcalcRef;
str batstrRef;
str batmtimeRef;
str batmmathRef;
str batxmlRef;
str batsqlRef;
str betweenRef;
str betweensymmetricRef;
str blockRef;
str bbpRef;
str tidRef;
str dateRef;
str deltaRef;
str subdeltaRef;
str projectdeltaRef;
str binddbatRef;
str bindidxRef;
str bindRef;
str emptybindRef;
str emptybindidxRef;
str bpmRef;
str bstreamRef;
str calcRef;
str clear_tableRef;
str closeRef;
str columnRef;
str columnBindRef;
str commitRef;
str connectRef;
str countRef;
str subcountRef;
str copyRef;
str copy_fromRef;
str export_tableRef;
str count_no_nilRef;
str crossRef;
str createRef;
str create_seqRef;
str create_schemaRef;
str create_viewRef;
str create_tableRef;
str create_constraintRef;
str create_typeRef;
str create_userRef;
str create_roleRef;
str create_userRef;
str create_indexRef;
str create_functionRef;
str create_triggerRef;
str dataflowRef;
str dblRef;
str defineRef;
str deleteRef;
str differenceRef;
str drop_seqRef;
str intersectRef;
str drop_schemaRef;
str drop_viewRef;
str drop_tableRef;
str drop_constraintRef;
str drop_typeRef;
str drop_userRef;
str drop_roleRef;
str drop_userRef;
str drop_indexRef;
str drop_functionRef;
str drop_triggerRef;
str subdiffRef;
str diffRef;
str subinterRef;
str mergecandRef;
str mergepackRef;
str intersectcandRef;
str eqRef;
str disconnectRef;
str evalRef;
str execRef;
str expandRef;
str exportOperationRef;
str findRef;
str finishRef;
str firstnRef;
str getRef;
str getTraceRef;
str generatorRef;
str grantRef;
str grant_rolesRef;
str grant_functionRef;
str groupRef;
str groupdoneRef;
str subgroupRef;
str subgroupdoneRef;
str groupbyRef;
str hgeRef;
str hashRef;
str identityRef;
str ifthenelseRef;
str inplaceRef;
str intRef;
str ioRef;
str iteratorRef;
str jitRef;
str jsonRef;
str joinRef;
str semijoinRef;
str leftjoinRef;
str antijoinRef;
str bandjoinRef;
str rangejoinRef;
str thetajoinRef;
str languageRef;
str projectionRef;
str projectionpathRef;
str likeselectRef;
str likethetaselectRef;
str ilikeselectRef;
str ilikethetaselectRef;
str likeRef;
str ilikeRef;
str not_likeRef;
str not_ilikeRef;
str listRef;
str lockRef;
str lookupRef;
str malRef;
str batmalRef;
str mapiRef;
str markRef;
str mtimeRef;
str multicolumnRef;
str matRef;
str max_no_nilRef;
str maxRef;
str submaxRef;
str submedianRef;
str mdbRef;
str min_no_nilRef;
str minRef;
str subminRef;
str mirrorRef;
str mitosisRef;
str mkeyRef;
str mmathRef;
str multiplexRef;
str manifoldRef;
str mvcRef;
str newRef;
str notRef;
str nextRef;
str oltpRef;
str oidRef;
str openRef;
str optimizerRef;
str parametersRef;
str packRef;
str pack2Ref;
str passRef;
str partitionRef;
str pcreRef;
str pinRef;
str singleRef;
str plusRef;
str minusRef;
str mulRef;
str divRef;
str printRef;
str preludeRef;
str prodRef;
str subprodRef;
str postludeRef;
str profilerRef;
str projectRef;
str putRef;
str querylogRef;
str queryRef;
str rapiRef;
str batrapiRef;
str pyapiRef;
str batpyapiRef;
str pyapimapRef;
str pyapi3Ref;
str batpyapi3Ref;
str pyapi3mapRef;
str subeval_aggrRef;
str rankRef;
str dense_rankRef;
str raiseRef;
str reconnectRef;
str refineRef;
str registerRef;
str releaseRef;
str remapRef;
str remoteRef;
str rename_userRef;
str replaceRef;
str replicatorRef;
str resultSetRef;
str reuseRef;
str revoke_rolesRef;
str revoke_functionRef;
str revokeRef;
str row_numberRef;
str rpcRef;
str rsColumnRef;
str schedulerRef;
str selectNotNilRef;
str seriesRef;
str semaRef;
str setAccessRef;
str setVariableRef;
str setWriteModeRef;
str sinkRef;
str sliceRef;
str subsliceRef;
str sortRef;
str sortReverseRef;
str sqlRef;
str sqlcatalogRef;
str streamsRef;
str startRef;
str starttraceRef;
str stopRef;
str stoptraceRef;
str strRef;
str sumRef;
str subsumRef;
str subavgRef;
str selectRef;
str sortRef;
str takeRef;
str transactionRef;
str transaction_beginRef;
str transaction_releaseRef;
str transaction_commitRef;
str transaction_abortRef;
str transaction_rollbackRef;
str not_uniqueRef;
str sampleRef;
str uniqueRef;
str subuniformRef;
str unlockRef;
str unpackRef;
str unpinRef;
str updateRef;
str timestampRef;
str thetaselectRef;
str likeselectRef;
str ilikeselectRef;
str userRef;
str vectorRef;
str zero_or_oneRef;

void optimizerInit(void)
{
	abortRef = putName("abort");
	affectedRowsRef = putName("affectedRows");
	aggrRef = putName("aggr");
	alarmRef = putName("alarm");
	algebraRef = putName("algebra");
	andRef = putName("and");
	appendidxRef = putName("append_idxbat");
	appendRef = putName("append");
	assertRef = putName("assert");
	attachRef = putName("attach");
	alter_seqRef = putName("alter_seq");
	alter_schemaRef = putName("alter_schema");
	alter_viewRef = putName("alter_view");
	alter_tableRef = putName("alter_table");
	alter_constraintRef = putName("alter_constraint");
	alter_typeRef = putName("alter_type");
	alter_userRef = putName("alter_user");
	alter_roleRef = putName("alter_role");
	alter_userRef = putName("alter_user");
	alter_indexRef = putName("alter_index");
	alter_functionRef = putName("alter_function");
	alter_triggerRef = putName("alter_trigger");
	alter_add_tableRef = putName("alter_add_table");
	alter_del_tableRef = putName("alter_del_table");
	alter_set_tableRef = putName("alter_set_table");
	avgRef = putName("avg");
	arrayRef = putName("array");
	batRef = putName("bat");
	batalgebraRef = putName("batalgebra");
	batcalcRef = putName("batcalc");
	basketRef = putName("basket");
	batstrRef = putName("batstr");
	batmtimeRef = putName("batmtime");
	batmmathRef = putName("batmmath");
	batxmlRef = putName("batxml");
	batsqlRef = putName("batsql");
	betweenRef = putName("between");
	betweensymmetricRef = putName("betweensymmetric");
	blockRef = putName("block");
	bbpRef = putName("bbp");
	tidRef = putName("tid");
	deltaRef = putName("delta");
	subdeltaRef = putName("subdelta");
	projectdeltaRef = putName("projectdelta");
	binddbatRef = putName("bind_dbat");
	bindidxRef = putName("bind_idxbat");
	bindRef = putName("bind");
	emptybindRef = putName("emptybind");
	emptybindidxRef = putName("emptybindidx");
	bpmRef = putName("bpm");
	bstreamRef = putName("bstream");
	calcRef = putName("calc");
	clear_tableRef = putName("clear_table");
	closeRef = putName("close");
	columnRef = putName("column");
	columnBindRef = putName("columnBind");
	commitRef = putName("commit");
	connectRef = putName("connect");
	countRef = putName("count");
	subcountRef = putName("subcount");
	copyRef = putName("copy");
	copy_fromRef = putName("copy_from");
	export_tableRef = putName("export_table");
	count_no_nilRef = putName("count_no_nil");
	crossRef = putName("crossproduct");
	createRef = putName("create");
	create_seqRef = putName("create_seq");
	create_schemaRef = putName("create_schema");
	create_viewRef = putName("create_view");
	create_tableRef = putName("create_table");
	create_constraintRef = putName("create_constraint");
	create_typeRef = putName("create_type");
	create_userRef = putName("create_user");
	create_roleRef = putName("create_role");
	create_userRef = putName("create_user");
	create_indexRef = putName("create_index");
	create_functionRef = putName("create_function");
	create_triggerRef = putName("create_trigger");
	dateRef = putName("date");
	dataflowRef = putName("dataflow");
	dblRef = putName("dbl");
	defineRef = putName("define");
	deleteRef = putName("delete");
	differenceRef = putName("difference");
	intersectRef = putName("intersect");
	drop_seqRef = putName("drop_seq");
	drop_schemaRef = putName("drop_schema");
	drop_viewRef = putName("drop_view");
	drop_tableRef = putName("drop_table");
	drop_constraintRef = putName("drop_constraint");
	drop_typeRef = putName("drop_type");
	drop_userRef = putName("drop_user");
	drop_roleRef = putName("drop_role");
	drop_userRef = putName("drop_user");
	drop_indexRef = putName("drop_index");
	drop_functionRef = putName("drop_function");
	drop_triggerRef = putName("drop_trigger");
	mergecandRef= putName("mergecand");
	mergepackRef= putName("mergepack");
	intersectcandRef= putName("intersectcand");
	eqRef = putName("==");
	disconnectRef= putName("disconnect");
	evalRef = putName("eval");
	execRef = putName("exec");
	expandRef = putName("expand");
	exportOperationRef = putName("exportOperation");
	findRef = putName("find");
	finishRef = putName("finish");
	firstnRef = putName("firstn");
	getRef = putName("get");
	getTraceRef = putName("getTrace");
	generatorRef = putName("generator");
	grantRef = putName("grant");
	grant_rolesRef = putName("grant_roles");
	grant_functionRef = putName("grant_function");
	groupRef = putName("group");
	groupdoneRef = putName("groupdone");
	subgroupRef = putName("subgroup");
	subgroupdoneRef= putName("subgroupdone");
	groupbyRef = putName("groupby");
	hgeRef = putName("hge");
	hashRef = putName("hash");
	identityRef = putName("identity");
	ifthenelseRef = putName("ifthenelse");
	inplaceRef = putName("inplace");
	intRef = putName("int");
	ioRef = putName("io");
	iteratorRef = putName("iterator");
	projectionpathRef = putName("projectionpath");
	joinRef = putName("join");
	semijoinRef = putName("semijoin");
	leftjoinRef = putName("leftjoin");
	antijoinRef = putName("antijoin");
	bandjoinRef = putName("bandjoin");
	rangejoinRef = putName("rangejoin");
	thetajoinRef = putName("thetajoin");
	jitRef = putName("jit");
	jsonRef = putName("json");
	languageRef= putName("language");
	projectionRef = putName("projection");
	likeselectRef = putName("likeselect");
	listRef = putName("list");
	likeRef = putName("like");
	ilikeRef = putName("ilike");
	ilikeselectRef = putName("ilikeselect");
	likethetaselectRef = putName("likethetaselect");
	ilikethetaselectRef = putName("ilikethetaselect");
	not_likeRef = putName("not_like");
	not_ilikeRef = putName("not_ilike");
	lockRef = putName("lock");
	lookupRef = putName("lookup");
	malRef = putName("mal");
	batmalRef = putName("batmal");
	mapiRef = putName("mapi");
	markRef = putName("mark");
	mtimeRef = putName("mtime");
	multicolumnRef = putName("multicolumn");
	matRef = putName("mat");
	max_no_nilRef = putName("max_no_nil");
	maxRef = putName("max");
	submaxRef = putName("submax");
	submedianRef = putName("submedian");
	mdbRef = putName("mdb");
	min_no_nilRef = putName("min_no_nil");
	minRef = putName("min");
	subminRef = putName("submin");
	mirrorRef = putName("mirror");
	mitosisRef = putName("mitosis");
	mkeyRef = putName("mkey");
	mmathRef = putName("mmath");
	multiplexRef = putName("multiplex");
	manifoldRef = putName("manifold");
	mvcRef = putName("mvc");
	newRef = putName("new");
	notRef = putName("not");
	nextRef = putName("next");
	oltpRef = putName("oltp");
	oidRef = putName("oid");
	optimizerRef = putName("optimizer");
	openRef = putName("open");
	parametersRef = putName("parameters");
	packRef = putName("pack");
	pack2Ref = putName("pack2");
	passRef = putName("pass");
	partitionRef = putName("partition");
	pcreRef = putName("pcre");
	pinRef = putName("pin");
	plusRef = putName("+");
	minusRef = putName("-");
	mulRef = putName("*");
	divRef = putName("/");
	printRef = putName("print");
	preludeRef = putName("prelude");
	prodRef = putName("prod");
	subprodRef = putName("subprod");
	profilerRef = putName("profiler");
	postludeRef = putName("postlude");
	projectRef = putName("project");
	putRef = putName("put");
	querylogRef = putName("querylog");
	queryRef = putName("query");
	rapiRef = putName("rapi");
	batrapiRef = putName("batrapi");
    pyapiRef = putName("pyapi");
    batpyapiRef = putName("batpyapi");
    pyapimapRef = putName("batpyapimap");
    pyapi3Ref = putName("pyapi3");
    batpyapi3Ref = putName("batpyapi3");
    pyapi3mapRef = putName("batpyapi3map");
	subeval_aggrRef = putName("subeval_aggr");
	rankRef = putName("rank");
	dense_rankRef = putName("dense_rank");
	raiseRef = putName("raise");
	reconnectRef = putName("reconnect");
	refineRef = putName("refine");
	registerRef = putName("register");
	releaseRef = putName("release");
	remapRef = putName("remap");
	remoteRef = putName("remote");
	rename_userRef = putName("rename_user");
	replaceRef = putName("replace");
	replicatorRef = putName("replicator");
	resultSetRef = putName("resultSet");
	revokeRef = putName("revoke");
	reuseRef = putName("reuse");
	revoke_rolesRef = putName("revoke_roles");
	revoke_functionRef = putName("revoke_function");
	row_numberRef = putName("row_number");
	rpcRef = putName("rpc");
	rsColumnRef = putName("rsColumn");
	schedulerRef = putName("scheduler");
	selectNotNilRef = putName("selectNotNil");
	seriesRef = putName("series");
	semaRef = putName("sema");
	setAccessRef = putName("setAccess");
	setVariableRef = putName("setVariable");
	setWriteModeRef= putName("setWriteMode");
	sinkRef = putName("sink");
	sliceRef = putName("slice");
	subsliceRef = putName("subslice");
	singleRef = putName("single");
	sortRef = putName("sort");
	sortReverseRef = putName("sortReverse");
	sqlRef = putName("sql");
	sqlcatalogRef = putName("sqlcatalog");
	streamsRef = putName("streams");
	startRef = putName("start");
	starttraceRef = putName("starttrace");
	stopRef = putName("stop");
	stoptraceRef = putName("stoptrace");
	strRef = putName("str");
	sumRef = putName("sum");
	subsumRef = putName("subsum");
	subavgRef = putName("subavg");
	sortRef = putName("sort");
	takeRef= putName("take");
	transactionRef= putName("transaction");
	transaction_beginRef= putName("transaction_begin");
	transaction_releaseRef= putName("transaction_release");
	transaction_commitRef= putName("transaction_commit");
	transaction_abortRef= putName("transaction_abort");
	transaction_rollbackRef= putName("transaction_rollback");
	timestampRef = putName("timestamp");
	not_uniqueRef= putName("not_unique");
	sampleRef= putName("sample");
	uniqueRef= putName("unique");
	subuniformRef= putName("subuniform");
	unlockRef= putName("unlock");
	unpackRef = putName("unpack");
	unpinRef = putName("unpin");
	updateRef = putName("update");
	userRef = putName("user");
	selectRef = putName("select");
	thetaselectRef = putName("thetaselect");
	vectorRef = putName("vector");
	zero_or_oneRef = putName("zero_or_one");
}
