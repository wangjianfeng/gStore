/*=============================================================================
# Filename:		Database.cpp
# Author: Bookug Lobert
# Mail: 1181955272@qq.com
# Last Modified:	2016-09-11 15:27
# Description: originally written by liyouhuan, modified by zengli and chenjiaqi
=============================================================================*/

#include "Database.h"

using namespace std;

Database::Database()
{
	this->name = "";
	string store_path = ".";
	this->signature_binary_file = "signature.binary";
	this->six_tuples_file = "six_tuples";
	this->db_info_file = "db_info_file.dat";

	string kv_store_path = store_path + "/kv_store";
	this->kvstore = new KVstore(kv_store_path);

	string vstree_store_path = store_path + "/vs_store";
	this->vstree = new VSTree(vstree_store_path);

	string stringindex_store_path = store_path + "/stringindex_store";
	this->stringindex = new StringIndex(stringindex_store_path);

	this->encode_mode = Database::STRING_MODE;
	this->is_active = false;
	this->sub_num = 0;
	this->pre_num = 0;
	this->literal_num = 0;
	this->entity_num = 0;
	this->triples_num = 0;

	this->join = NULL;
	this->pre2num = NULL;
	this->entity_buffer = NULL;
	this->entity_buffer_size = 0;
	this->literal_buffer = NULL;
	this->literal_buffer_size = 0;

	//this->resetIDinfo();
	this->initIDinfo();
}

Database::Database(string _name)
{
	this->name = _name;
	string store_path = this->name;

	this->signature_binary_file = "signature.binary";
	this->six_tuples_file = "six_tuples";
	this->db_info_file = "db_info_file.dat";

	string kv_store_path = store_path + "/kv_store";
	this->kvstore = new KVstore(kv_store_path);

	string vstree_store_path = store_path + "/vs_store";
	this->vstree = new VSTree(vstree_store_path);

	string stringindex_store_path = store_path + "/stringindex_store";
	this->stringindex = new StringIndex(stringindex_store_path);

	this->encode_mode = Database::STRING_MODE;
	this->is_active = false;
	this->sub_num = 0;
	this->pre_num = 0;
	this->literal_num = 0;
	this->entity_num = 0;
	this->triples_num = 0;

	this->join = NULL;
	this->pre2num = NULL;
	this->entity_buffer = NULL;
	this->entity_buffer_size = 0;
	this->literal_buffer = NULL;
	this->literal_buffer_size = 0;

	//this->resetIDinfo();
	this->initIDinfo();
}

//==================================================================================================================================================
//NOTICE:
//there are 3 ways to manage a dynamic-garbage ID list
//1. push all unused IDs into list, get top each time to alloc, push freed one to tail, push more large one if NULL
//(can use bit array if stored in disk)
//2. when free, change the mapping for non-free one whose ID is the largest currently
//(sometimes maybe the copy cost is very high)
//3. NULL first and push one if freed, get one if not empty(limit+1 if NULL)
//(when stored in disk, maybe consume larger space)
//However, this method can keep the used largest easily(no!! 1->2->3, delete 2 and delete 3, then the max 1 is not kept by limit!)

void
Database::initIDinfo()
{
	//NOTICE:keep that limit-1 the maxium using ID
	this->free_id_file_entity = this->getStorePath() + "/freeEntityID.dat";
	this->limitID_entity = 0;
	this->freelist_entity = NULL;

	this->free_id_file_literal = this->getStorePath() + "/freeLiteralID.dat";
	this->limitID_literal = 0;
	this->freelist_literal = NULL;

	this->free_id_file_predicate = this->getStorePath() + "/freePredicateID.dat";
	this->limitID_predicate = 0;
	this->freelist_predicate = NULL;
}

void
Database::resetIDinfo()
{
	this->initIDinfo();

	//this->limitID_entity = Database::START_ID_NUM;
	//NOTICE:add base LITERAL_FIRST_ID for literals
	//this->limitID_literal = Database::START_ID_NUM;
	//this->limitID_predicate = Database::START_ID_NUM;

	//BlockInfo* tmp = NULL;
	//for (int i = Database::START_ID_NUM - 1; i >= 0; --i)
	//{
	//tmp = new BlockInfo(i, this->freelist_entity);
	//this->freelist_entity = tmp;
	//tmp = new BlockInfo(i, this->freelist_literal);
	//this->freelist_literal = tmp;
	//tmp = new BlockInfo(i, this->freelist_predicate);
	//this->freelist_predicate = tmp;
	//}
}

void
Database::readIDinfo()
{
	this->initIDinfo();

	FILE* fp = NULL;
	int t = -1;
	BlockInfo* bp = NULL;

	fp = fopen(this->free_id_file_entity.c_str(), "r");
	if (fp == NULL)
	{
		cout << "read entity id info error" << endl;
		return;
	}
	//QUERY:this will reverse the original order, if change?
	//Notice that if we cannot ensure that IDs are uporder and continuous, we can
	//not keep an array for IDs like _entity_bitset
	BlockInfo *tmp = NULL, *cur = NULL;
	fread(&(this->limitID_entity), sizeof(int), 1, fp);
	fread(&t, sizeof(int), 1, fp);
	while (!feof(fp))
	{
		//if(t == 14912)
		//{
		//cout<<"Database::readIDinfo() - get 14912"<<endl;
		//}
		//bp = new BlockInfo(t, this->freelist_entity);
		//this->freelist_entity = bp;
		tmp = new BlockInfo(t);
		if (cur == NULL)
		{
			this->freelist_entity = cur = tmp;
		}
		else
		{
			cur->next = tmp;
			cur = tmp;
		}
		fread(&t, sizeof(int), 1, fp);
	}
	fclose(fp);
	fp = NULL;


	fp = fopen(this->free_id_file_literal.c_str(), "r");
	if (fp == NULL)
	{
		cout << "read literal id info error" << endl;
		return;
	}
	fread(&(this->limitID_literal), sizeof(int), 1, fp);
	fread(&t, sizeof(int), 1, fp);
	while (!feof(fp))
	{
		bp = new BlockInfo(t, this->freelist_literal);
		this->freelist_literal = bp;
		fread(&t, sizeof(int), 1, fp);
	}
	fclose(fp);
	fp = NULL;

	fp = fopen(this->free_id_file_predicate.c_str(), "r");
	if (fp == NULL)
	{
		cout << "read predicate id info error" << endl;
		return;
	}
	fread(&(this->limitID_predicate), sizeof(int), 1, fp);
	fread(&t, sizeof(int), 1, fp);
	while (!feof(fp))
	{
		bp = new BlockInfo(t, this->freelist_predicate);
		this->freelist_predicate = bp;
		fread(&t, sizeof(int), 1, fp);
	}
	fclose(fp);
	fp = NULL;
}

void
Database::writeIDinfo()
{
	//cout<<"now to write the id info"<<endl;
	FILE* fp = NULL;
	BlockInfo *bp = NULL, *tp = NULL;

	fp = fopen(this->free_id_file_entity.c_str(), "w+");
	if (fp == NULL)
	{
		cout << "write entity id info error" << endl;
		return;
	}
	fwrite(&(this->limitID_entity), sizeof(int), 1, fp);
	bp = this->freelist_entity;
	while (bp != NULL)
	{
		//if(bp->num == 14912)
		//{
		//cout<<"Database::writeIDinfo() - get 14912"<<endl;
		//}
		fwrite(&(bp->num), sizeof(int), 1, fp);
		tp = bp->next;
		delete bp;
		bp = tp;
	}
	fclose(fp);
	fp = NULL;

	fp = fopen(this->free_id_file_literal.c_str(), "w+");
	if (fp == NULL)
	{
		cout << "write literal id info error" << endl;
		return;
	}
	fwrite(&(this->limitID_literal), sizeof(int), 1, fp);
	bp = this->freelist_literal;
	while (bp != NULL)
	{
		fwrite(&(bp->num), sizeof(int), 1, fp);
		tp = bp->next;
		delete bp;
		bp = tp;
	}
	fclose(fp);
	fp = NULL;

	fp = fopen(this->free_id_file_predicate.c_str(), "w+");
	if (fp == NULL)
	{
		cout << "write predicate id info error" << endl;
		return;
	}
	fwrite(&(this->limitID_predicate), sizeof(int), 1, fp);
	bp = this->freelist_predicate;
	while (bp != NULL)
	{
		fwrite(&(bp->num), sizeof(int), 1, fp);
		tp = bp->next;
		delete bp;
		bp = tp;
	}
	fclose(fp);
	fp = NULL;
}

//ID alloc garbage error(LITERAL_FIRST_ID or double) add base for literal
int
Database::allocEntityID()
{
	int t;
	if (this->freelist_entity == NULL)
	{
		t = this->limitID_entity++;
		if (this->limitID_entity >= Util::LITERAL_FIRST_ID)
		{
			cout << "fail to alloc id for entity" << endl;
			return -1;
		}
	}
	else
	{
		t = this->freelist_entity->num;
		BlockInfo* op = this->freelist_entity;
		this->freelist_entity = this->freelist_entity->next;
		delete op;
	}

	this->entity_num++;
	return t;
}

void
Database::freeEntityID(int _id)
{
	if (_id == this->limitID_entity - 1)
	{
		this->limitID_entity--;
	}
	else
	{
		BlockInfo* p = new BlockInfo(_id, this->freelist_entity);
		this->freelist_entity = p;
	}

	this->entity_num--;
}

int
Database::allocLiteralID()
{
	int t;
	if (this->freelist_literal == NULL)
	{
		t = this->limitID_literal++;
		if (this->limitID_literal >= Util::LITERAL_FIRST_ID)
		{
			cout << "fail to alloc id for literal" << endl;
			return -1;
		}
	}
	else
	{
		t = this->freelist_literal->num;
		BlockInfo* op = this->freelist_literal;
		this->freelist_literal = this->freelist_literal->next;
		delete op;
	}

	this->literal_num++;
	return t + Util::LITERAL_FIRST_ID;
}

void
Database::freeLiteralID(int _id)
{
	if (_id == this->limitID_literal - 1)
	{
		this->limitID_literal--;
	}
	else
	{
		BlockInfo* p = new BlockInfo(_id - Util::LITERAL_FIRST_ID, this->freelist_literal);
		this->freelist_literal = p;
	}

	this->literal_num--;
}

int
Database::allocPredicateID()
{
	int t;
	if (this->freelist_predicate == NULL)
	{
		t = this->limitID_predicate++;
		if (this->limitID_predicate >= Util::LITERAL_FIRST_ID)
		{
			cout << "fail to alloc id for predicate" << endl;
			return -1;
		}
	}
	else
	{
		t = this->freelist_predicate->num;
		BlockInfo* op = this->freelist_predicate;
		this->freelist_predicate = this->freelist_predicate->next;
		delete op;
	}

	this->pre_num++;
	return t;
}

void
Database::freePredicateID(int _id)
{
	if (_id == this->limitID_predicate - 1)
	{
		this->limitID_predicate--;
	}
	else
	{
		BlockInfo* p = new BlockInfo(_id, this->freelist_predicate);
		this->freelist_predicate = p;
	}

	this->pre_num--;
}

void
Database::release(FILE* fp0)
{
	fprintf(fp0, "begin to delete DB!\n");
	fflush(fp0);
	this->vstree->saveTree();
	delete this->vstree;
	fprintf(fp0, "ok to delete vstree!\n");
	fflush(fp0);
	delete this->kvstore;
	fprintf(fp0, "ok to delete kvstore!\n");
	fflush(fp0);
	//fclose(Util::debug_database);
	//Util::debug_database = NULL;	//debug: when multiple databases
	fprintf(fp0, "ok to delete DB!\n");
	fflush(fp0);
}

Database::~Database()
{
	this->unload();
	//fclose(Util::debug_database);
	//Util::debug_database = NULL;	//debug: when multiple databases
}

void
Database::setPreMap()
{
	this->maxNumPID = this->minNumPID = -1;
	int max = 0, min = this->triples_num + 1;
	this->pre2num = new TNUM[this->limitID_predicate];
	int valid = 0, i, t;
	for (i = 0; i < this->limitID_predicate; ++i)
	{
		if (valid == this->pre_num)
		{
			t = 0;
		}
		else
		{
			t = this->kvstore->getPredicateDegree(i);
		}
		this->pre2num[i] = t;

		//NOTICE:only when pre2num[i]>0 then i is a valid predicate id
		if (t > 0)
		{
			valid++;
			if (t > max)
			{
				this->maxNumPID = i;
			}
			if (t < min)
			{
				this->minNumPID = i;
			}
		}
	}
}

void
Database::setStringBuffer()
{
	//BETTER?maybe different size for entity and literal, maybe different offset should be used
	this->entity_buffer_size = (this->limitID_entity<50000000) ? this->limitID_entity : 50000000;
	this->literal_buffer_size = (this->limitID_literal<50000000) ? this->limitID_literal : 50000000;
	this->entity_buffer = new Buffer(this->entity_buffer_size);
	this->literal_buffer = new Buffer(this->literal_buffer_size);

	int valid = 0, i;
	string str;
	for (i = 0; i < this->entity_buffer_size; ++i)
	{
		if (valid == this->entity_num)
		{
			str = "";
		}
		else
		{
			str = this->kvstore->getEntityByID(i);
		}
		this->entity_buffer->set(i, str);
		if (str != "")
		{
			valid++;
		}
	}

	valid = 0;
	for (i = 0; i < this->literal_buffer_size; ++i)
	{
		if (valid == this->literal_num)
		{
			str = "";
		}
		else
		{
			str = this->kvstore->getLiteralByID(i + Util::LITERAL_FIRST_ID);
		}
		this->literal_buffer->set(i, str);
		if (str != "")
		{
			valid++;
		}
	}

	//use this string buffer in StringIndex class, if not exist, then also use StringIndex to search
	this->stringindex->setBuffer(this->entity_buffer, this->literal_buffer);
}

void
Database::warmUp()
{
	//the most frequent triple
	int pid1 = this->maxNumPID;
	ResultSet rs1;
	string str1 = "select ?s ?o where { ?s " + this->kvstore->getPredicateByID(pid1) + " ?o . }";
	this->query(str1, rs1);
	//the most infrequent triple
	int pid2 = this->minNumPID;
	ResultSet rs2;
	string str2 = "select ?s ?o where { ?s " + this->kvstore->getPredicateByID(pid2) + " ?o . }";
	this->query(str2, rs2);
}

bool
Database::load()
{
	//DEBUG:what if loaded several times?to check if loaded?
	bool flag = (this->vstree)->loadTree();
	if (!flag)
	{
		cout << "load tree error. @Database::load()" << endl;
		return false;
	}

	flag = this->loadDBInfoFile();
	if (!flag)
	{
		cout << "load database info error. @Database::load()" << endl;
		return false;
	}

	(this->kvstore)->open();

	this->stringindex->load();

	this->readIDinfo();

	this->setPreMap();
	//generate the string buffer for entity and literal, no need for predicate
	//NOTICE:the total string size should not exceed 10G, assume that most strings length < 500
	//too many empty between entity and literal, so divide them
	this->setStringBuffer();

	//warm up always as finishing build(), to utilize the system buffer
	//this->warmUp();
	//DEBUG:the warmUp() calls query(), which will also output results, this is not we want

	cout << "finish load" << endl;

	return true;
}

bool
Database::unload()
{
	delete[] this->pre2num;
	this->pre2num = NULL;
	delete this->entity_buffer;
	delete this->literal_buffer;

	this->vstree->saveTree();
	delete this->vstree;
	this->vstree = NULL;
	delete this->kvstore;
	this->kvstore = NULL;
	delete this->stringindex;
	this->stringindex = NULL;

	this->saveDBInfoFile();
	this->writeIDinfo();
	this->initIDinfo();
	return true;
}

string
Database::getName()
{
	return this->name;
}

int
Database::query(const string _query, ResultSet& _result_set, FILE* _fp)
{
	GeneralEvaluation general_evaluation(this->vstree, this->kvstore, this->stringindex, this->pre2num, this->limitID_predicate, this->limitID_literal);

	long tv_begin = Util::get_cur_time();

	if (!general_evaluation.parseQuery(_query))
		return false;
	long tv_parse = Util::get_cur_time();
	cout << "after Parsing, used " << (tv_parse - tv_begin) << "ms." << endl;

	//TODO:output all results in JSON format, and transformed into string to client
	//for select, -100 by default, -101 means error
	//for update, non-negative means true(and the num is updated triples num), -1 means error
	int success_num = -100;  

	//Query
	if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Not_Update)
	{
		bool query_ret = general_evaluation.doQuery();
		if(!query_ret)
		{
			success_num = -101;
		}

		long tv_bfget = Util::get_cur_time();
		general_evaluation.getFinalResult(_result_set);
		long tv_afget = Util::get_cur_time();
		cout << "after getFinalResult, used " << (tv_afget - tv_bfget) << "ms." << endl;

		if(_fp != NULL)
			general_evaluation.setNeedOutputAnswer();
	}
	//Update
	else
	{
		success_num = 0;
		TripleWithObjType *update_triple = NULL;
		int update_triple_num = 0;

		if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Insert_Data || general_evaluation.getQueryTree().getUpdateType() == QueryTree::Delete_Data)
		{
			QueryTree::GroupPattern &update_pattern = general_evaluation.getQueryTree().getUpdateType() == QueryTree::Insert_Data ?
				general_evaluation.getQueryTree().getInsertPatterns() : general_evaluation.getQueryTree().getDeletePatterns();

			update_triple_num = update_pattern.patterns.size();
			update_triple = new TripleWithObjType[update_triple_num];

			for (int i = 0; i < (int)update_pattern.patterns.size(); i++)
			{
				TripleWithObjType::ObjectType object_type = TripleWithObjType::None;
				if (update_pattern.patterns[i].object.value[0] == '<')
					object_type = TripleWithObjType::Entity;
				else
					object_type = TripleWithObjType::Literal;

				update_triple[i] = TripleWithObjType(update_pattern.patterns[i].subject.value,
					update_pattern.patterns[i].predicate.value,
					update_pattern.patterns[i].object.value, object_type);
			}

			if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Insert_Data)
			{
				success_num = insert(update_triple, update_triple_num);
			}
			else if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Delete_Data)
			{
				success_num = remove(update_triple, update_triple_num);
			}
		}
		else if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Delete_Where || general_evaluation.getQueryTree().getUpdateType() == QueryTree::Insert_Clause ||
			general_evaluation.getQueryTree().getUpdateType() == QueryTree::Delete_Clause || general_evaluation.getQueryTree().getUpdateType() == QueryTree::Modify_Clause)
		{
			general_evaluation.getQueryTree().setProjectionAsterisk();
			general_evaluation.doQuery();

			if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Delete_Where || general_evaluation.getQueryTree().getUpdateType() == QueryTree::Delete_Clause || general_evaluation.getQueryTree().getUpdateType() == QueryTree::Modify_Clause)
			{
				general_evaluation.prepareUpdateTriple(general_evaluation.getQueryTree().getDeletePatterns(), update_triple, update_triple_num);
				success_num = remove(update_triple, update_triple_num);
			}
			if (general_evaluation.getQueryTree().getUpdateType() == QueryTree::Insert_Clause || general_evaluation.getQueryTree().getUpdateType() == QueryTree::Modify_Clause)
			{
				general_evaluation.prepareUpdateTriple(general_evaluation.getQueryTree().getInsertPatterns(), update_triple, update_triple_num);
				success_num = insert(update_triple, update_triple_num);
			}
		}

		general_evaluation.releaseResultStack();
		delete[] update_triple;
	}

	long tv_final = Util::get_cur_time();
	cout << "Total time used: " << (tv_final - tv_begin) << "ms." << endl;

	if (general_evaluation.needOutputAnswer())
	{
		int ans_num = max(_result_set.ansNum - _result_set.output_offset, 0);
		if (_result_set.output_limit != -1)
			ans_num = min(ans_num, _result_set.output_limit);
		cout << "There has answer: " << ans_num << endl;
		cout << "final result is : " << endl;
		_result_set.output(_fp);
		fprintf(_fp, "\n");
		fflush(_fp);       //to empty the output buffer in C (fflush(stdin) not work in GCC)
	}

#ifdef DEBUG
	cout<<"query success_num: "<<success_num<<endl;
#endif
	return success_num;
}

//NOTICE+QUERY:to save memory for large cases, we can consider building one tree at a time(then release)
//Or read the rdf file on separate segments
//WARN:the ID type is int, and entity/literal are just separated by a limit
//which means that entity num <= 10^9 literal num <= 10^9 predicate num <= 2*10^9
//If we use unsigned as type, then max triple can be 10^9(edge case)
//If we use long long, no more problem, but wasteful
//Or we can consider divide entity and literal totally
//In distributed gStore, each machine's graph should be based on unique encoding IDs,
//and require that triples in each graph no more than a limit(maybe 10^9)
bool
Database::build(const string& _rdf_file)
{
	//manage the id for a new database
	this->resetIDinfo();

	string ret = Util::getExactPath(_rdf_file.c_str());
	long tv_build_begin = Util::get_cur_time();

	string store_path = this->name;
	Util::create_dir(store_path);

	string kv_store_path = store_path + "/kv_store";
	Util::create_dir(kv_store_path);

	string vstree_store_path = store_path + "/vs_store";
	Util::create_dir(vstree_store_path);

	string stringindex_store_path = store_path + "/stringindex_store";
	Util::create_dir(stringindex_store_path);

	cout << "begin encode RDF from : " << ret << " ..." << endl;

	//BETTER+TODO:now require that dataset size < memory
	//to support really larger datasets, divide and insert into B+ tree and VStree
	//(read the value, add list and set; update the signature, remove and reinsert)
	//the query process is nearly the same
	//QUERY:build each index in one thread to speed up, but the sorting case?
	//one sorting order for several indexes

	// to be switched to new encodeRDF method.
	//    this->encodeRDF(ret);
	if (!this->encodeRDF_new(ret))	//<-- this->kvstore->id2* trees are closed
	{
		return false;
	}
	cout << "finish encode." << endl;

	//cout<<"test kv"<<this->kvstore->getIDByPredicate("<contain>")<<endl;

	//this->kvstore->flush();
	delete this->kvstore;
	this->kvstore = NULL;
	//sync();
	//cout << "sync kvstore" << endl;
	//this->kvstore->release();
	//cout<<"release kvstore"<<endl;

	long before_vstree = Util::get_cur_time();
	//(this->kvstore)->open();
	string _entry_file = this->getSignatureBFile();

	cout << "begin build VS-Tree on " << ret << "..." << endl;
	(this->vstree)->buildTree(_entry_file);

	long tv_build_end = Util::get_cur_time();

	cout << "after build vstree, used " << (tv_build_end - before_vstree) << "ms." << endl;
	cout << "after build, used " << (tv_build_end - tv_build_begin) << "ms." << endl;
	cout << "finish build VS-Tree." << endl;

	//this->vstree->saveTree();
	//delete this->vstree;
	//this->vstree = NULL;
	//sync();
	//cout << "sync vstree" << endl;

	//string cmd = "rm -rf " + _entry_file;
	//system(cmd.c_str());
	//cout << "signature file removed" << endl;

	return true;
}

//root Path of this DB + sixTuplesFile
string
Database::getSixTuplesFile()
{
	return this->getStorePath() + "/" + this->six_tuples_file;
}

/* root Path of this DB + signatureBFile */
string
Database::getSignatureBFile()
{
	return this->getStorePath() + "/" + this->signature_binary_file;
}

/* root Path of this DB + DBInfoFile */
string
Database::getDBInfoFile()
{
	return this->getStorePath() + "/" + this->db_info_file;
}

bool
Database::saveDBInfoFile()
{
	FILE* filePtr = fopen(this->getDBInfoFile().c_str(), "wb");

	if (filePtr == NULL)
	{
		cout << "error, can not create db info file. @Database::saveDBInfoFile" << endl;
		return false;
	}

	fseek(filePtr, 0, SEEK_SET);

	fwrite(&this->triples_num, sizeof(int), 1, filePtr);
	fwrite(&this->entity_num, sizeof(int), 1, filePtr);
	fwrite(&this->sub_num, sizeof(int), 1, filePtr);
	fwrite(&this->pre_num, sizeof(int), 1, filePtr);
	fwrite(&this->literal_num, sizeof(int), 1, filePtr);
	fwrite(&this->encode_mode, sizeof(int), 1, filePtr);
	fclose(filePtr);

	Util::triple_num = this->triples_num;
	Util::pre_num = this->pre_num;
	Util::entity_num = this->entity_num;
	Util::literal_num = this->literal_num;

	return true;
}

bool
Database::loadDBInfoFile()
{
	FILE* filePtr = fopen(this->getDBInfoFile().c_str(), "rb");

	if (filePtr == NULL)
	{
		cout << "error, can not open db info file. @Database::loadDBInfoFile" << endl;
		return false;
	}

	fseek(filePtr, 0, SEEK_SET);

	fread(&this->triples_num, sizeof(int), 1, filePtr);
	fread(&this->entity_num, sizeof(int), 1, filePtr);
	fread(&this->sub_num, sizeof(int), 1, filePtr);
	fread(&this->pre_num, sizeof(int), 1, filePtr);
	fread(&this->literal_num, sizeof(int), 1, filePtr);
	fread(&this->encode_mode, sizeof(int), 1, filePtr);
	fclose(filePtr);

	Util::triple_num = this->triples_num;
	Util::pre_num = this->pre_num;
	Util::entity_num = this->entity_num;
	Util::literal_num = this->literal_num;

	return true;
}

string
Database::getStorePath()
{
	return this->name;
}

//encode relative signature data of the query graph
void
Database::buildSparqlSignature(SPARQLquery & _sparql_q)
{
	vector<BasicQuery*>& _query_union = _sparql_q.getBasicQueryVec();
	for (unsigned int i_bq = 0; i_bq < _query_union.size(); i_bq++)
	{
		BasicQuery* _basic_q = _query_union[i_bq];
		_basic_q->encodeBasicQuery(this->kvstore, _sparql_q.getQueryVar());
	}
}

bool
Database::calculateEntityBitSet(int _entity_id, EntityBitSet & _bitset)
{
	int _list_len = 0;
	//when as subject
	int* _polist = NULL;
	(this->kvstore)->getpreIDobjIDlistBysubID(_entity_id, _polist, _list_len);
	Triple _triple;
	_triple.subject = (this->kvstore)->getEntityByID(_entity_id);
	for (int i = 0; i < _list_len; i += 2)
	{
		int _pre_id = _polist[i];
		int _obj_id = _polist[i + 1];
		_triple.object = (this->kvstore)->getEntityByID(_obj_id);
		if (_triple.object == "")
		{
			_triple.object = (this->kvstore)->getLiteralByID(_obj_id);
		}
		_triple.predicate = (this->kvstore)->getPredicateByID(_pre_id);
		this->encodeTriple2SubEntityBitSet(_bitset, &_triple);
	}
	delete[] _polist;

	//when as object
	int* _pslist = NULL;
	_list_len = 0;
	(this->kvstore)->getpreIDsubIDlistByobjID(_entity_id, _pslist, _list_len);
	_triple.object = (this->kvstore)->getEntityByID(_entity_id);
	for (int i = 0; i < _list_len; i += 2)
	{
		int _pre_id = _pslist[i];
		int _sub_id = _pslist[i + 1];
		_triple.subject = (this->kvstore)->getEntityByID(_sub_id);
		_triple.predicate = (this->kvstore)->getPredicateByID(_pre_id);
		this->encodeTriple2ObjEntityBitSet(_bitset, &_triple);
	}
	delete[] _pslist;

	return true;
}

//encode Triple into subject SigEntry
bool
Database::encodeTriple2SubEntityBitSet(EntityBitSet& _bitset, const Triple* _p_triple)
{
	int _pre_id = -1;
	{
		_pre_id = (this->kvstore)->getIDByPredicate(_p_triple->predicate);
		//BETTER: checking whether _pre_id is -1 or not will be more reliable
	}

	Signature::encodePredicate2Entity(_pre_id, _bitset, Util::EDGE_OUT);
	if (this->encode_mode == Database::ID_MODE)
	{
		//TODO
	}
	else if (this->encode_mode == Database::STRING_MODE)
	{
		Signature::encodeStr2Entity((_p_triple->object).c_str(), _bitset);
	}

	return true;
}

//encode Triple into object SigEntry
bool
Database::encodeTriple2ObjEntityBitSet(EntityBitSet& _bitset, const Triple* _p_triple)
{
	int _pre_id = -1;
	{
		_pre_id = (this->kvstore)->getIDByPredicate(_p_triple->predicate);
		//BETTER: checking whether _pre_id is -1 or not will be more reliable
	}

	Signature::encodePredicate2Entity(_pre_id, _bitset, Util::EDGE_IN);
	if (this->encode_mode == Database::ID_MODE)
	{
		//TODO
	}
	else if (this->encode_mode == Database::STRING_MODE)
	{
		Signature::encodeStr2Entity((_p_triple->subject).c_str(), _bitset);
	}

	return true;
}

//check whether the relative 3-tuples exist usually, through sp2olist
bool
Database::exist_triple(int _sub_id, int _pre_id, int _obj_id)
{
	int* _objidlist = NULL;
	int _list_len = 0;
	(this->kvstore)->getobjIDlistBysubIDpreID(_sub_id, _pre_id, _objidlist, _list_len);

	bool is_exist = false;
	//	for(int i = 0; i < _list_len; i ++)
	//	{
	//		if(_objidlist[i] == _obj_id)
	//		{
	//		    is_exist = true;
	//			break;
	//		}
	//	}
	if (Util::bsearch_int_uporder(_obj_id, _objidlist, _list_len) != -1)
	{
		is_exist = true;
	}
	delete[] _objidlist;

	return is_exist;
}

//NOTICE: all constants are transfered to ids in memory
//this maybe not ok when size is too large!
bool
Database::encodeRDF_new(const string _rdf_file)
{
#ifdef DEBUG
	//cout<< "now to log!!!" << endl;
	Util::logging("In encodeRDF_new");
	//cout<< "end log!!!" << endl;
#endif
	int** _p_id_tuples = NULL;
	int _id_tuples_max = 0;

	long t1 = Util::get_cur_time();

	//map sub2id, pre2id, entity/literal in obj2id, store in kvstore, encode RDF data into signature
	if (!this->sub2id_pre2id_obj2id_RDFintoSignature(_rdf_file, _p_id_tuples, _id_tuples_max))
	{
		return false;
	}

	long t2 = Util::get_cur_time();
	cout << "after encode, used " << (t2 - t1) << "ms." << endl;

	//build stringindex before this->kvstore->id2* trees are closed
	this->stringindex->setNum(StringIndexFile::Entity, this->entity_num);
	this->stringindex->setNum(StringIndexFile::Literal, this->literal_num);
	this->stringindex->setNum(StringIndexFile::Predicate, this->pre_num);
	this->stringindex->save(*this->kvstore);

	//NOTICE:close these trees now to save memory
	this->kvstore->close_entity2id();
	this->kvstore->close_id2entity();
	this->kvstore->close_literal2id();
	this->kvstore->close_id2literal();
	this->kvstore->close_predicate2id();
	this->kvstore->close_id2predicate();

	this->kvstore->build_subID2values(_p_id_tuples, this->triples_num);
	long t3 = Util::get_cur_time();
	cout << "after s2xx, used " << (t3 - t2) << "ms." << endl;

	this->kvstore->build_objID2values(_p_id_tuples, this->triples_num);
	long t4 = Util::get_cur_time();
	cout << "after o2xx, used " << (t4 - t3) << "ms." << endl;

	this->kvstore->build_preID2values(_p_id_tuples, this->triples_num);
	long t5 = Util::get_cur_time();
	cout << "after p2xx, used " << (t5 - t4) << "ms." << endl;

	//WARN:we must free the memory for id_tuples array
	for (int i = 0; i < this->triples_num; ++i)
	{
		delete[] _p_id_tuples[i];
	}
	delete[] _p_id_tuples;

	bool flag = this->saveDBInfoFile();
	if (!flag)
	{
		return false;
	}

	Util::logging("finish encodeRDF_new");

	return true;
}

bool
Database::sub2id_pre2id_obj2id_RDFintoSignature(const string _rdf_file, int**& _p_id_tuples, int & _id_tuples_max)
{
	int _id_tuples_size;
	{
		//initial
		_id_tuples_max = 10 * 1000 * 1000;
		_p_id_tuples = new int*[_id_tuples_max];
		_id_tuples_size = 0;
		this->sub_num = 0;
		this->pre_num = 0;
		this->entity_num = 0;
		this->literal_num = 0;
		this->triples_num = 0;
		(this->kvstore)->open_entity2id(KVstore::CREATE_MODE);
		(this->kvstore)->open_id2entity(KVstore::CREATE_MODE);
		(this->kvstore)->open_predicate2id(KVstore::CREATE_MODE);
		(this->kvstore)->open_id2predicate(KVstore::CREATE_MODE);
		(this->kvstore)->open_literal2id(KVstore::CREATE_MODE);
		(this->kvstore)->open_id2literal(KVstore::CREATE_MODE);
	}

	//Util::logging("finish initial sub2id_pre2id_obj2id");
	cout << "finish initial sub2id_pre2id_obj2id" << endl;

	//BETTER?:close the stdio buffer sync??
	ifstream _fin(_rdf_file.c_str());
	if (!_fin)
	{
		cout << "sub2id&pre2id&obj2id: Fail to open : " << _rdf_file << endl;
		//exit(0);
		return false;
	}

	string _six_tuples_file = this->getSixTuplesFile();
	ofstream _six_tuples_fout(_six_tuples_file.c_str());
	if (!_six_tuples_fout)
	{
		cout << "sub2id&pre2id&obj2id: Fail to open: " << _six_tuples_file << endl;
		//exit(0);
		return false;
	}

	TripleWithObjType* triple_array = new TripleWithObjType[RDFParser::TRIPLE_NUM_PER_GROUP];

	//don't know the number of entity
	//pre allocate entitybitset_max EntityBitSet for storing signature, double the space until the _entity_bitset is used up.
	int entitybitset_max = 10000;
	EntityBitSet** _entity_bitset = new EntityBitSet*[entitybitset_max];
	for (int i = 0; i < entitybitset_max; i++)
	{
		_entity_bitset[i] = new EntityBitSet();
		_entity_bitset[i]->reset();
	}
	EntityBitSet _tmp_bitset;

	//parse a file
	RDFParser _parser(_fin);

	Util::logging("==> while(true)");

	while (true)
	{
		int parse_triple_num = 0;

		_parser.parseFile(triple_array, parse_triple_num);

		{
			stringstream _ss;
			_ss << "finish rdfparser" << this->triples_num << endl;
			//Util::logging(_ss.str());
			cout << _ss.str() << endl;
		}
		cout << "after info in sub2id_" << endl;

		if (parse_triple_num == 0)
		{
			break;
		}

		//Process the Triple one by one
		for (int i = 0; i < parse_triple_num; i++)
		{
			this->triples_num++;

			//if the _id_tuples exceeds, double the space
			if (_id_tuples_size == _id_tuples_max)
			{
				int _new_tuples_len = _id_tuples_max * 2;
				int** _new_id_tuples = new int*[_new_tuples_len];
				memcpy(_new_id_tuples, _p_id_tuples, sizeof(int*) * _id_tuples_max);
				delete[] _p_id_tuples;
				_p_id_tuples = _new_id_tuples;
				_id_tuples_max = _new_tuples_len;
			}

			// For subject
			// (all subject is entity, some object is entity, the other is literal)
			string _sub = triple_array[i].getSubject();
			int _sub_id = (this->kvstore)->getIDByEntity(_sub);
			if (_sub_id == -1)
			{
				//_sub_id = this->entity_num;
				_sub_id = this->allocEntityID();
				//this->entity_num++;
				(this->kvstore)->setIDByEntity(_sub, _sub_id);
				(this->kvstore)->setEntityByID(_sub_id, _sub);
			}
			//  For predicate
			string _pre = triple_array[i].getPredicate();
			int _pre_id = (this->kvstore)->getIDByPredicate(_pre);
			if (_pre_id == -1)
			{
				//_pre_id = this->pre_num;
				_pre_id = this->allocPredicateID();
				//this->pre_num++;
				(this->kvstore)->setIDByPredicate(_pre, _pre_id);
				(this->kvstore)->setPredicateByID(_pre_id, _pre);
			}

			//  For object
			string _obj = triple_array[i].getObject();
			int _obj_id = -1;
			// obj is entity
			if (triple_array[i].isObjEntity())
			{
				_obj_id = (this->kvstore)->getIDByEntity(_obj);
				if (_obj_id == -1)
				{
					//_obj_id = this->entity_num;
					_obj_id = this->allocEntityID();
					//this->entity_num++;
					(this->kvstore)->setIDByEntity(_obj, _obj_id);
					(this->kvstore)->setEntityByID(_obj_id, _obj);
				}
			}
			//obj is literal
			if (triple_array[i].isObjLiteral())
			{
				_obj_id = (this->kvstore)->getIDByLiteral(_obj);
				if (_obj_id == -1)
				{
					//_obj_id = Util::LITERAL_FIRST_ID + (this->literal_num);
					_obj_id = this->allocLiteralID();
					//this->literal_num++;
					(this->kvstore)->setIDByLiteral(_obj, _obj_id);
					(this->kvstore)->setLiteralByID(_obj_id, _obj);
					//#ifdef DEBUG
					//if(_obj == "\"Bob\"")
					//{
					//cout << "this is id for Bob: " << _obj_id << endl;
					//}
					//cout<<"literal should be bob: " << kvstore->getLiteralByID(_obj_id)<<endl;
					//cout<<"id for bob: "<<kvstore->getIDByLiteral("\"Bob\"")<<endl;
					//#endif
				}
			}

			//  For id_tuples
			_p_id_tuples[_id_tuples_size] = new int[3];
			_p_id_tuples[_id_tuples_size][0] = _sub_id;
			_p_id_tuples[_id_tuples_size][1] = _pre_id;
			_p_id_tuples[_id_tuples_size][2] = _obj_id;
			_id_tuples_size++;

#ifdef DEBUG_PRECISE
			//  save six tuples
			{
				_six_tuples_fout << _sub_id << '\t'
					<< _pre_id << '\t'
					<< _obj_id << '\t'
					<< _sub << '\t'
					<< _pre << '\t'
					<< _obj << endl;
			}
#endif
			//_entity_bitset is used up, double the space
			if (this->entity_num >= entitybitset_max)
			{
				//cout<<"to double entity bitset num"<<endl;
				EntityBitSet** _new_entity_bitset = new EntityBitSet*[entitybitset_max * 2];
				memcpy(_new_entity_bitset, _entity_bitset, sizeof(EntityBitSet*) * entitybitset_max);
				delete[] _entity_bitset;
				_entity_bitset = _new_entity_bitset;

				int tmp = entitybitset_max * 2;
				for (int i = entitybitset_max; i < tmp; i++)
				{
					_entity_bitset[i] = new EntityBitSet();
					_entity_bitset[i]->reset();
				}

				entitybitset_max = tmp;
			}

			{
				_tmp_bitset.reset();
				Signature::encodePredicate2Entity(_pre_id, _tmp_bitset, Util::EDGE_OUT);
				Signature::encodeStr2Entity(_obj.c_str(), _tmp_bitset);
				*_entity_bitset[_sub_id] |= _tmp_bitset;
			}

			if (triple_array[i].isObjEntity())
			{
				_tmp_bitset.reset();
				Signature::encodePredicate2Entity(_pre_id, _tmp_bitset, Util::EDGE_IN);
				Signature::encodeStr2Entity(_sub.c_str(), _tmp_bitset);
				//cout<<"objid: "<<_obj_id <<endl;
				//when 15999 error
				//WARN:id allocated can be very large while the num is not so much
				//This means that maybe the space for the _obj_id is not allocated now
				//NOTICE:we prepare the free id list in uporder and contiguous, so in build process
				//this can work well
				*_entity_bitset[_obj_id] |= _tmp_bitset;
			}
		}
	}

	Util::logging("==> end while(true)");

	delete[] triple_array;
	_fin.close();
	_six_tuples_fout.close();


	{
		//save all entity_signature into binary file
		string _sig_binary_file = this->getSignatureBFile();
		FILE* _sig_fp = fopen(_sig_binary_file.c_str(), "wb");
		if (_sig_fp == NULL) {
			cout << "Failed to open : " << _sig_binary_file << endl;
		}

		//EntityBitSet _all_bitset;
		for (int i = 0; i < this->entity_num; i++)
		{
			SigEntry* _sig = new SigEntry(EntitySig(*_entity_bitset[i]), i);
			fwrite(_sig, sizeof(SigEntry), 1, _sig_fp);
			//_all_bitset |= *_entity_bitset[i];
			delete _sig;
		}
		fclose(_sig_fp);

		for (int i = 0; i < entitybitset_max; i++)
		{
			delete _entity_bitset[i];
		}
		delete[] _entity_bitset;
	}

	{
		stringstream _ss;
		_ss << "finish sub2id pre2id obj2id" << endl;
		_ss << "tripleNum is " << this->triples_num << endl;
		_ss << "entityNum is " << this->entity_num << endl;
		_ss << "preNum is " << this->pre_num << endl;
		_ss << "literalNum is " << this->literal_num << endl;
		Util::logging(_ss.str());
		cout << _ss.str() << endl;
	}

	return true;
}

bool
Database::insertTriple(const TripleWithObjType& _triple, vector<int>* _vertices, vector<int>* _predicates)
{
	//cout<<endl<<"the new triple is:"<<endl;
	//cout<<_triple.subject<<endl;
	//cout<<_triple.predicate<<endl;
	//cout<<_triple.object<<endl;

	//int sid1 = this->kvstore->getIDByEntity("<http://www.Department7.University0.edu/UndergraduateStudent394>");
	//int sid2 = this->kvstore->getIDByEntity("<http://www.Department7.University0.edu/UndergraduateStudent395>");
	//int oid1 = this->kvstore->getIDByEntity("<ub:UndergraduateStudent>");
	//int oid2 = this->kvstore->getIDByEntity("<UndergraduateStudent394@Department7.University0.edu>");
	//cout<<sid1<<" "<<sid2<<" "<<oid1<<" "<<oid2<<endl;

	long tv_kv_store_begin = Util::get_cur_time();

	int _sub_id = (this->kvstore)->getIDByEntity(_triple.subject);
	bool _is_new_sub = false;
	//if sub does not exist
	if (_sub_id == -1)
	{
		_is_new_sub = true;
		//_sub_id = this->entity_num++;
		_sub_id = this->allocEntityID();
		//cout<<"this is a new sub id"<<endl;
		//if(_sub_id == 14912)
		//{
		//cout<<"get the error one"<<endl;
		//cout<<_sub_id<<endl<<_triple.subject<<endl;
		//cout<<_triple.predicate<<endl<<_triple.object<<endl;
		//}
		this->sub_num++;
		(this->kvstore)->setIDByEntity(_triple.subject, _sub_id);
		(this->kvstore)->setEntityByID(_sub_id, _triple.subject);

		//update the string buffer
		if (_sub_id < this->entity_buffer_size)
		{
			this->entity_buffer->set(_sub_id, _triple.subject);
		}

		if (_vertices != NULL)
			_vertices->push_back(_sub_id);
	}

	int _pre_id = (this->kvstore)->getIDByPredicate(_triple.predicate);
	bool _is_new_pre = false;
	//if pre does not exist
	if (_pre_id == -1)
	{
		_is_new_pre = true;
		//_pre_id = this->pre_num++;
		_pre_id = this->allocPredicateID();
		(this->kvstore)->setIDByPredicate(_triple.predicate, _pre_id);
		(this->kvstore)->setPredicateByID(_pre_id, _triple.predicate);

		if (_predicates != NULL)
			_predicates->push_back(_pre_id);
	}

	//object is either entity or literal
	int _obj_id = -1;
	bool _is_new_obj = false;
	bool is_obj_entity = _triple.isObjEntity();
	if (is_obj_entity)
	{
		_obj_id = (this->kvstore)->getIDByEntity(_triple.object);

		if (_obj_id == -1)
		{
			_is_new_obj = true;
			//_obj_id = this->entity_num++;
			_obj_id = this->allocEntityID();
			(this->kvstore)->setIDByEntity(_triple.object, _obj_id);
			(this->kvstore)->setEntityByID(_obj_id, _triple.object);

			//update the string buffer
			if (_obj_id < this->entity_buffer_size)
			{
				this->entity_buffer->set(_obj_id, _triple.object);
			}

			if (_vertices != NULL)
				_vertices->push_back(_obj_id);
		}
	}
	else
	{
		_obj_id = (this->kvstore)->getIDByLiteral(_triple.object);

		if (_obj_id == -1)
		{
			_is_new_obj = true;
			//_obj_id = Util::LITERAL_FIRST_ID + this->literal_num;
			_obj_id = this->allocLiteralID();
			(this->kvstore)->setIDByLiteral(_triple.object, _obj_id);
			(this->kvstore)->setLiteralByID(_obj_id, _triple.object);

			//update the string buffer
			int tid = _obj_id - Util::LITERAL_FIRST_ID;
			if (tid < this->literal_buffer_size)
			{
				this->literal_buffer->set(tid, _triple.object);
			}

			if (_vertices != NULL)
				_vertices->push_back(_obj_id);
		}
	}

	//if this is not a new triple, return directly
	bool _triple_exist = false;
	if (!_is_new_sub && !_is_new_pre && !_is_new_obj)
	{
		_triple_exist = this->exist_triple(_sub_id, _pre_id, _obj_id);
	}

	//debug
	//  {
	//      stringstream _ss;
	//      _ss << this->literal_num << endl;
	//      _ss <<"ids: " << _sub_id << " " << _pre_id << " " << _obj_id << " " << _triple_exist << endl;
	//      Util::logging(_ss.str());
	//  }

	if (_triple_exist)
	{
		cout << "this triple already exist" << endl;
		return false;
	}
	else
	{
		this->triples_num++;
	}
	//cout<<"the triple spo ids: "<<_sub_id<<" "<<_pre_id<<" "<<_obj_id<<endl;

	//update sp2o op2s s2po o2ps s2o o2s etc.
	int updateLen = (this->kvstore)->updateTupleslist_insert(_sub_id, _pre_id, _obj_id);

	//int* list = NULL;
	//int len = 0;
	//int root = this->kvstore->getIDByEntity("<root>");
	//int contain = this->kvstore->getIDByPredicate("<contain>");
	//this->kvstore->getobjIDlistBysubIDpreID(root, contain, list, len);
	//cout<<root<<" "<<contain<<endl;
	//if(len == 0)
	//{
	//cout<<"no result"<<endl;
	//}
	//for(int i = 0; i < len; ++i)
	//{
	//cout << this->kvstore->getEntityByID(list[i])<<" "<<list[i]<<endl;
	//}

	long tv_kv_store_end = Util::get_cur_time();

	EntityBitSet _sub_entity_bitset;
	_sub_entity_bitset.reset();

	this->encodeTriple2SubEntityBitSet(_sub_entity_bitset, &_triple);

	//if new entity then insert it, else update it.
	if (_is_new_sub)
	{
		cout<<"to insert: "<<_sub_id<<" "<<this->kvstore->getEntityByID(_sub_id)<<endl;
		SigEntry _sig(_sub_id, _sub_entity_bitset);
		(this->vstree)->insertEntry(_sig);
	}
	else
	{
		//cout<<"to update: "<<_sub_id<<" "<<this->kvstore->getEntityByID(_sub_id)<<endl;
		(this->vstree)->updateEntry(_sub_id, _sub_entity_bitset);
	}

	//if the object is an entity, then update or insert this entity's entry.
	if (is_obj_entity)
	{
		EntityBitSet _obj_entity_bitset;
		_obj_entity_bitset.reset();

		this->encodeTriple2ObjEntityBitSet(_obj_entity_bitset, &_triple);

		if (_is_new_obj)
		{
			//cout<<"to insert: "<<_obj_id<<" "<<this->kvstore->getEntityByID(_obj_id)<<endl;
			SigEntry _sig(_obj_id, _obj_entity_bitset);
			(this->vstree)->insertEntry(_sig);
		}
		else
		{
			//cout<<"to update: "<<_obj_id<<" "<<this->kvstore->getEntityByID(_obj_id)<<endl;
			(this->vstree)->updateEntry(_obj_id, _obj_entity_bitset);
		}
	}

	long tv_vs_store_end = Util::get_cur_time();

	//debug
	{
		cout << "update kv_store, used " << (tv_kv_store_end - tv_kv_store_begin) << "ms." << endl;
		cout << "update vs_store, used " << (tv_vs_store_end - tv_kv_store_end) << "ms." << endl;
	}

	return true;
	//return updateLen;
}

bool
Database::removeTriple(const TripleWithObjType& _triple, vector<int>* _vertices, vector<int>* _predicates)
{
	long tv_kv_store_begin = Util::get_cur_time();

	int _sub_id = (this->kvstore)->getIDByEntity(_triple.subject);
	int _pre_id = (this->kvstore)->getIDByPredicate(_triple.predicate);
	int _obj_id = (this->kvstore)->getIDByEntity(_triple.object);
	if (_obj_id == -1)
	{
		_obj_id = (this->kvstore)->getIDByLiteral(_triple.object);
	}

	if (_sub_id == -1 || _pre_id == -1 || _obj_id == -1)
	{
		return false;
	}
	bool _exist_triple = this->exist_triple(_sub_id, _pre_id, _obj_id);
	if (!_exist_triple)
	{
		return false;
	}
	else
	{
		this->triples_num--;
	}

	cout << "triple existence checked" << endl;

	//remove from sp2o op2s s2po o2ps s2o o2s
	//sub2id, pre2id and obj2id will not be updated
	(this->kvstore)->updateTupleslist_remove(_sub_id, _pre_id, _obj_id);
	cout << "11 trees updated" << endl;

	long tv_kv_store_end = Util::get_cur_time();

	int sub_degree = (this->kvstore)->getEntityDegree(_sub_id);
	//if subject become an isolated point, remove its corresponding entry
	if (sub_degree == 0)
	{
		cout<<"to remove entry for sub"<<endl;
		cout<<_sub_id << " "<<this->kvstore->getEntityByID(_sub_id)<<endl;
		this->kvstore->subEntityByID(_sub_id);
		this->kvstore->subIDByEntity(_triple.subject);
		(this->vstree)->removeEntry(_sub_id);
		this->freeEntityID(_sub_id);
		this->sub_num--;
		//update the string buffer
		if (_sub_id < this->entity_buffer_size)
		{
			this->entity_buffer->del(_sub_id);
		}
		if (_vertices != NULL)
			_vertices->push_back(_sub_id);
	}
	//else re-calculate the signature of subject & replace that in vstree
	else
	{
		//cout<<"to replace entry for sub"<<endl;
		//cout<<_sub_id << " "<<this->kvstore->getEntityByID(_sub_id)<<endl;
		EntityBitSet _entity_bitset;
		_entity_bitset.reset();
		this->calculateEntityBitSet(_sub_id, _entity_bitset);
		//NOTICE:can not use updateEntry as insert because this is in remove
		//In insert we can add a OR operation and all is ok
		(this->vstree)->replaceEntry(_sub_id, _entity_bitset);
	}
	//cout<<"subject dealed"<<endl;

	bool is_obj_entity = _triple.isObjEntity();
	int obj_degree;
	if (is_obj_entity)
	{
		obj_degree = this->kvstore->getEntityDegree(_obj_id);
		if (obj_degree == 0)
		{
			//cout<<"to remove entry for obj"<<endl;
			//cout<<_obj_id << " "<<this->kvstore->getEntityByID(_obj_id)<<endl;
			this->kvstore->subEntityByID(_obj_id);
			this->kvstore->subIDByEntity(_triple.object);
			this->vstree->removeEntry(_obj_id);
			this->freeEntityID(_obj_id);
			//update the string buffer
			if (_obj_id < this->entity_buffer_size)
			{
				this->entity_buffer->del(_obj_id);
			}
			if (_vertices != NULL)
				_vertices->push_back(_obj_id);
		}
		else
		{
			//cout<<"to replace entry for obj"<<endl;
			//cout<<_obj_id << " "<<this->kvstore->getEntityByID(_obj_id)<<endl;
			EntityBitSet _entity_bitset;
			_entity_bitset.reset();
			this->calculateEntityBitSet(_obj_id, _entity_bitset);
			this->vstree->replaceEntry(_obj_id, _entity_bitset);
		}
	}
	else
	{
		obj_degree = this->kvstore->getLiteralDegree(_obj_id);
		if (obj_degree == 0)
		{
			this->kvstore->subLiteralByID(_obj_id);
			this->kvstore->subIDByLiteral(_triple.object);
			this->freeLiteralID(_obj_id);
			//update the string buffer
			int tid = _obj_id - Util::LITERAL_FIRST_ID;
			if (tid < this->literal_buffer_size)
			{
				this->literal_buffer->del(tid);
			}
			if (_vertices != NULL)
				_vertices->push_back(_obj_id);
		}
	}
	//cout<<"object dealed"<<endl;

	int pre_degree = this->kvstore->getPredicateDegree(_pre_id);
	if (pre_degree == 0)
	{
		this->kvstore->subPredicateByID(_pre_id);
		this->kvstore->subIDByPredicate(_triple.predicate);
		this->freePredicateID(_pre_id);
		if (_predicates != NULL)
			_predicates->push_back(_pre_id);
	}
	//cout<<"predicate dealed"<<endl;

	long tv_vs_store_end = Util::get_cur_time();

	//debug
	{
		cout << "update kv_store, used " << (tv_kv_store_end - tv_kv_store_begin) << "ms." << endl;
		cout << "update vs_store, used " << (tv_vs_store_end - tv_kv_store_end) << "ms." << endl;
	}
	return true;
}

bool
Database::insert(std::string _rdf_file)
{
	bool flag = this->load();
	if (!flag)
	{
		return false;
	}
	cout << "finish loading" << endl;

	long tv_load = Util::get_cur_time();

	int success_num = 0;

	ifstream _fin(_rdf_file.c_str());
	if (!_fin)
	{
		cout << "fail to open : " << _rdf_file << ".@insert_test" << endl;
		//exit(0);
		return false;
	}

	//NOTICE+WARN:we can not load all triples into memory all at once!!!
	//the parameter in build and insert must be the same, because RDF parser also use this
	//for build process, this one can be big enough if memory permits
	//for insert/delete process, this can not be too large, otherwise too costly
	TripleWithObjType* triple_array = new TripleWithObjType[RDFParser::TRIPLE_NUM_PER_GROUP];
	//parse a file
	RDFParser _parser(_fin);

	int triple_num = 0;
#ifdef DEBUG
	Util::logging("==> while(true)");
#endif
	while (true)
	{
		int parse_triple_num = 0;
		_parser.parseFile(triple_array, parse_triple_num);
#ifdef DEBUG
		stringstream _ss;
		//NOTICE:this is not same as others, use parse_triple_num directly
		_ss << "finish rdfparser" << parse_triple_num << endl;
		Util::logging(_ss.str());
		cout << _ss.str() << endl;
#endif
		if (parse_triple_num == 0)
		{
			break;
		}

		//Process the Triple one by one
		success_num += this->insert(triple_array, parse_triple_num);
		//some maybe invalid or duplicate
		//triple_num += parse_triple_num;
	}

	delete[] triple_array;
	long tv_insert = Util::get_cur_time();
	cout << "after insert, used " << (tv_insert - tv_load) << "ms." << endl;
	//BETTER:update kvstore and vstree separately, to lower the memory cost
	//flag = this->vstree->saveTree();
	//if (!flag)
	//{
		//return false;
	//}
	//flag = this->saveDBInfoFile();
	//if (!flag)
	//{
		//return false;
	//}

	cout << "insert rdf triples done." << endl;
	cout<<"inserted triples num: "<<success_num<<endl;

	//int* list = NULL;
	//int len = 0;
	//int root = this->kvstore->getIDByEntity("<root>");
	//int contain = this->kvstore->getIDByPredicate("<contain>");
	//this->kvstore->getobjIDlistBysubIDpreID(root, contain, list, len);
	//cout<<root<<" "<<contain<<endl;
	//if(len == 0)
	//{
	//cout<<"no result"<<endl;
	//}
	//for(int i = 0; i < len; ++i)
	//{
	//cout << this->kvstore->getEntityByID(list[i])<<" "<<list[i]<<endl;
	//}
	//cout<<endl;
	//int t = this->kvstore->getIDByEntity("<node5>");
	//cout<<t<<endl;
	//cout<<this->kvstore->getEntityByID(0)<<endl;

	return true;
}

bool
Database::remove(std::string _rdf_file)
{
	bool flag = this->load();
	if (!flag)
	{
		return false;
	}
	cout << "finish loading" << endl;

	long tv_load = Util::get_cur_time();
	int success_num = 0;

	ifstream _fin(_rdf_file.c_str());
	if (!_fin)
	{
		cout << "fail to open : " << _rdf_file << ".@remove_test" << endl;
		return false;
	}

	//NOTICE+WARN:we can not load all triples into memory all at once!!!
	TripleWithObjType* triple_array = new TripleWithObjType[RDFParser::TRIPLE_NUM_PER_GROUP];
	//parse a file
	RDFParser _parser(_fin);

	//int triple_num = 0;
#ifdef DEBUG
	Util::logging("==> while(true)");
#endif
	while (true)
	{
		int parse_triple_num = 0;
		_parser.parseFile(triple_array, parse_triple_num);
#ifdef DEBUG
		stringstream _ss;
		//NOTICE:this is not same as others, use parse_triple_num directly
		_ss << "finish rdfparser" << parse_triple_num << endl;
		Util::logging(_ss.str());
		cout << _ss.str() << endl;
#endif
		if (parse_triple_num == 0)
		{
			break;
		}

		success_num += this->remove(triple_array, parse_triple_num);
		//some maybe invalid or duplicate
		//triple_num -= parse_triple_num;
	}

	//TODO:better to free this just after id_tuples are ok
	//(only when using group insertion/deletion)
	//or reduce the array size
	delete[] triple_array;
	long tv_remove = Util::get_cur_time();
	cout << "after remove, used " << (tv_remove - tv_load) << "ms." << endl;

	//flag = this->vstree->saveTree();
	//if (!flag)
	//{
		//return false;
	//}
	//flag = this->saveDBInfoFile();
	//if (!flag)
	//{
		//return false;
	//}

	cout << "remove rdf triples done." << endl;
	cout<<"removed triples num: "<<success_num<<endl;

	return true;
}

int
Database::insert(const TripleWithObjType* _triples, int _triple_num)
{
	vector<int> vertices, predicates;
	int valid_num = 0;

#ifdef USE_GROUP_INSERT
	//NOTICE:this is called by insert(file) or query()(but can not be too large),
	//assume that db is loaded already
	int** id_tuples = new int*[_triple_num];
	int i = 0;
	//for(i = 0; i < _triple_num; ++i)
	//{
	//id_tuples[i] = new int[3];
	//}
	map<int, EntityBitSet> old_sigmap;
	map<int, EntityBitSet> new_sigmap;
	set<int> new_entity;
	map<int, EntityBitSet>::iterator it;
	EntityBitSet tmpset;
	tmpset.reset();

	int subid, objid, preid;
	bool is_obj_entity;
	for (i = 0; i < _triple_num; ++i)
	{
		bool is_new_sub = false, is_new_pre = false, is_new_obj = false;

		string sub = _triples[i].getSubject();
		subid = this->kvstore->getIDByEntity(sub);
		if (subid == -1)
		{
			is_new_sub = true;
			subid = this->allocEntityID();
			cout << "this is a new subject: " << sub << " " << subid << endl;
			this->sub_num++;
			this->kvstore->setIDByEntity(sub, subid);
			this->kvstore->setEntityByID(subid, sub);
			new_entity.insert(subid);
			//add info and update buffer
			vertices.push_back(subid);
			if (subid < this->entity_buffer_size)
			{
				this->entity_buffer->set(subid, sub);
			}
		}

		string pre = _triples[i].getPredicate();
		preid = this->kvstore->getIDByPredicate(pre);
		if (preid == -1)
		{
			is_new_pre = true;
			preid = this->allocPredicateID();
			this->kvstore->setIDByPredicate(pre, preid);
			this->kvstore->setPredicateByID(preid, pre);
			predicates.push_back(preid);
		}

		is_obj_entity = _triples[i].isObjEntity();
		string obj = _triples[i].getObject();
		if (is_obj_entity)
		{
			objid = this->kvstore->getIDByEntity(obj);
			if (objid == -1)
			{
				is_new_obj = true;
				objid = this->allocEntityID();
				cout << "this is a new object: " << obj << " " << objid << endl;
				//this->obj_num++;
				this->kvstore->setIDByEntity(obj, objid);
				this->kvstore->setEntityByID(objid, obj);
				new_entity.insert(objid);
				//add info and update
				vertices.push_back(objid);
				if (objid < this->entity_buffer_size)
				{
					this->entity_buffer->set(objid, obj);
				}
			}
		}
		else //isObjLiteral
		{
			objid = this->kvstore->getIDByLiteral(obj);
			if (objid == -1)
			{
				is_new_obj = true;
				objid = this->allocLiteralID();
				//this->obj_num++;
				this->kvstore->setIDByLiteral(obj, objid);
				this->kvstore->setLiteralByID(objid, obj);
				//add info and update
				vertices.push_back(objid);
				int tid = objid - Util::LITERAL_FIRST_ID;
				if (tid < this->literal_buffer_size)
				{
					this->literal_buffer->set(tid, obj);
				}
			}
		}

		bool triple_exist = false;
		if (!is_new_sub && !is_new_pre && !is_new_obj)
		{
			triple_exist = this->exist_triple(subid, preid, objid);
		}
		if (triple_exist)
		{
#ifdef DEBUG
			cout << "this triple exist" << endl;
#endif
			continue;
		}
#ifdef DEBUG
		cout << "this triple not exist" << endl;
#endif

		id_tuples[valid_num] = new int[3];
		id_tuples[valid_num][0] = subid;
		id_tuples[valid_num][1] = preid;
		id_tuples[valid_num][2] = objid;
		this->triples_num++;
		valid_num++;

		tmpset.reset();
		Signature::encodePredicate2Entity(preid, tmpset, Util::EDGE_OUT);
		Signature::encodeStr2Entity(obj.c_str(), tmpset);
		if (new_entity.find(subid) != new_entity.end())
		{
			it = new_sigmap.find(subid);
			if (it != new_sigmap.end())
			{
				it->second |= tmpset;
			}
			else
			{
				new_sigmap[subid] = tmpset;
			}
		}
		else
		{
			it = old_sigmap.find(subid);
			if (it != old_sigmap.end())
			{
				it->second |= tmpset;
			}
			else
			{
				old_sigmap[subid] = tmpset;
			}
		}

		if (is_obj_entity)
		{
			tmpset.reset();
			Signature::encodePredicate2Entity(preid, tmpset, Util::EDGE_IN);
			Signature::encodeStr2Entity(sub.c_str(), tmpset);
			if (new_entity.find(objid) != new_entity.end())
			{
				it = new_sigmap.find(objid);
				if (it != new_sigmap.end())
				{
					it->second |= tmpset;
				}
				else
				{
					new_sigmap[objid] = tmpset;
				}
			}
			else
			{
				it = old_sigmap.find(objid);
				if (it != old_sigmap.end())
				{
					it->second |= tmpset;
				}
				else
				{
					old_sigmap[objid] = tmpset;
				}
			}
		}
	}

#ifdef DEBUG
	cout << "old sigmap size: " << old_sigmap.size() << endl;
	cout << "new sigmap size: " << new_sigmap.size() << endl;
	cout << "valid num: " << valid_num << endl;
#endif

	//NOTICE:need to sort and remove duplicates, update the valid num
	//Notice that duplicates in a group can csuse problem
	//We finish this by spo cmp

	//this->kvstore->updateTupleslist_insert(_sub_id, _pre_id, _obj_id);
	//sort and update kvstore: 11 indexes
	//
	//BETTER:maybe also use int* here with a size to start
	//NOTICE:all kvtrees are opened now, one by one if memory is bottleneck
	//
	//spo cmp: s2p s2o s2po sp2o
	{
#ifdef DEBUG
		cout << "INSRET PROCESS: to spo cmp and update" << endl;
#endif
		qsort(id_tuples, valid_num, sizeof(int*), KVstore::_spo_cmp);

		//To remove duplicates
		//int ti = 1, tj = 1;
		//while(tj < valid_num)
		//{
		//if(id_tuples[tj][0] != id_tuples[tj-1][0] || id_tuples[tj][1] != id_tuples[tj-1][1] || id_tuples[tj][2] != id_tuples[tj-1][2])
		//{
		//id_tuples[ti][0] = id_tuples[tj][0];
		//id_tuples[ti][1] = id_tuples[tj][1];
		//id_tuples[ti][2] = id_tuples[tj][2];
		//ti++;
		//}
		//tj++;
		//}
		//for(tj = ti; tj < valid_num; ++tj)
		//{
		//delete[] id_tuples[tj];
		//id_tuples[tj] = NULL;
		//}
		//valid_num = ti;
		//
		//Notice that below already consider duplicates in loop

		vector<int> oidlist_s;
		vector<int> pidlist_s;
		vector<int> oidlist_sp;
		vector<int> pidoidlist_s;

		bool _sub_change = true;
		bool _sub_pre_change = true;
		bool _pre_change = true;

		for (int i = 0; i < valid_num; ++i)
			if (i + 1 == valid_num || (id_tuples[i][0] != id_tuples[i + 1][0] || id_tuples[i][1] != id_tuples[i + 1][1] || id_tuples[i][2] != id_tuples[i + 1][2]))
			{
				int _sub_id = id_tuples[i][0];
				int _pre_id = id_tuples[i][1];
				int _obj_id = id_tuples[i][2];

				oidlist_s.push_back(_obj_id);
				oidlist_sp.push_back(_obj_id);
				pidoidlist_s.push_back(_pre_id);
				pidoidlist_s.push_back(_obj_id);
				pidlist_s.push_back(_pre_id);

				_sub_change = (i + 1 == valid_num) || (id_tuples[i][0] != id_tuples[i + 1][0]);
				_pre_change = (i + 1 == valid_num) || (id_tuples[i][1] != id_tuples[i + 1][1]);
				_sub_pre_change = _sub_change || _pre_change;

				if (_sub_pre_change)
				{
#ifdef DEBUG
					cout << "update sp2o: " << _sub_id << " " << _pre_id << " " << oidlist_sp.size() << endl;
#endif
					cout << this->kvstore->getEntityByID(_sub_id) << endl;
					cout << this->kvstore->getPredicateByID(_pre_id) << endl;
					//this->kvstore->updateInsert_sp2o(_sub_id, _pre_id, oidlist_sp);
					oidlist_sp.clear();
				}

				if (_sub_change)
				{
#ifdef DEBUG
					cout << "update s2p: " << _sub_id << " " << pidlist_s.size() << endl;
#endif
					//this->kvstore->updateInsert_s2p(_sub_id, pidlist_s);
					pidlist_s.clear();

#ifdef DEBUG
					cout << "update s2po: " << _sub_id << " " << pidoidlist_s.size() << endl;
#endif
					this->kvstore->updateInsert_s2values(_sub_id, pidoidlist_s);
					pidoidlist_s.clear();

#ifdef DEBUG
					cout << "update s2o: " << _sub_id << " " << oidlist_s.size() << endl;
#endif
					sort(oidlist_s.begin(), oidlist_s.end());
					//this->kvstore->updateInsert_s2o(_sub_id, oidlist_s);
					oidlist_s.clear();
				}

			}
#ifdef DEBUG
		cout << "INSERT PROCESS: OUT s2po..." << endl;
#endif
	}
	//ops cmp: o2p o2s o2ps op2s
	{
#ifdef DEBUG
		cout << "INSRET PROCESS: to ops cmp and update" << endl;
#endif
		qsort(id_tuples, valid_num, sizeof(int**), KVstore::_ops_cmp);
		vector<int> sidlist_o;
		vector<int> sidlist_op;
		vector<int> pidsidlist_o;
		vector<int> pidlist_o;

		bool _obj_change = true;
		bool _pre_change = true;
		bool _obj_pre_change = true;

		for (int i = 0; i < valid_num; ++i)
			if (i + 1 == valid_num || (id_tuples[i][0] != id_tuples[i + 1][0] || id_tuples[i][1] != id_tuples[i + 1][1] || id_tuples[i][2] != id_tuples[i + 1][2]))
			{
				int _sub_id = id_tuples[i][0];
				int _pre_id = id_tuples[i][1];
				int _obj_id = id_tuples[i][2];

				sidlist_o.push_back(_sub_id);
				sidlist_op.push_back(_sub_id);
				pidsidlist_o.push_back(_pre_id);
				pidsidlist_o.push_back(_sub_id);
				pidlist_o.push_back(_pre_id);

				_obj_change = (i + 1 == valid_num) || (id_tuples[i][2] != id_tuples[i + 1][2]);
				_pre_change = (i + 1 == valid_num) || (id_tuples[i][1] != id_tuples[i + 1][1]);
				_obj_pre_change = _obj_change || _pre_change;

				if (_obj_pre_change)
				{
#ifdef DEBUG
					cout << "update op2s: " << _obj_id << " " << _pre_id << " " << sidlist_op.size() << endl;
#endif
					//this->kvstore->updateInsert_op2s(_obj_id, _pre_id, sidlist_op);
					sidlist_op.clear();
				}

				if (_obj_change)
				{
#ifdef DEBUG
					cout << "update o2s: " << _obj_id << " " << sidlist_o.size() << endl;
#endif
					sort(sidlist_o.begin(), sidlist_o.end());
					//this->kvstore->updateInsert_o2s(_obj_id, sidlist_o);
					sidlist_o.clear();

#ifdef DEBUG
					cout << "update o2ps: " << _obj_id << " " << pidsidlist_o.size() << endl;
#endif
					this->kvstore->updateInsert_o2values(_obj_id, pidsidlist_o);
					pidsidlist_o.clear();

#ifdef DEBUG
					cout << "update o2p: " << _obj_id << " " << pidlist_o.size() << endl;
#endif
					//this->kvstore->updateInsert_o2p(_obj_id, pidlist_o);
					pidlist_o.clear();
				}

			}
#ifdef DEBUG
		cout << "INSERT PROCESS: OUT o2ps..." << endl;
#endif
	}
	//pso cmp: p2s p2o p2so
	{
#ifdef DEBUG
		cout << "INSRET PROCESS: to pso cmp and update" << endl;
#endif
		qsort(id_tuples, valid_num, sizeof(int*), KVstore::_pso_cmp);
		vector<int> sidlist_p;
		vector<int> oidlist_p;
		vector<int> sidoidlist_p;

		bool _pre_change = true;
		bool _sub_change = true;
		//bool _pre_sub_change = true;

		for (int i = 0; i < valid_num; i++)
			if (i + 1 == valid_num || (id_tuples[i][0] != id_tuples[i + 1][0] || id_tuples[i][1] != id_tuples[i + 1][1] || id_tuples[i][2] != id_tuples[i + 1][2]))
			{
				int _sub_id = id_tuples[i][0];
				int _pre_id = id_tuples[i][1];
				int _obj_id = id_tuples[i][2];

				oidlist_p.push_back(_obj_id);
				sidoidlist_p.push_back(_sub_id);
				sidoidlist_p.push_back(_obj_id);
				sidlist_p.push_back(_sub_id);

				_pre_change = (i + 1 == valid_num) || (id_tuples[i][1] != id_tuples[i + 1][1]);
				_sub_change = (i + 1 == valid_num) || (id_tuples[i][0] != id_tuples[i + 1][0]);
				//_pre_sub_change = _pre_change || _sub_change;

				if (_pre_change)
				{
#ifdef DEBUG
					cout << "update p2s: " << _pre_id << " " << sidlist_p.size() << endl;
#endif
					//this->kvstore->updateInsert_p2s(_pre_id, sidlist_p);
					sidlist_p.clear();

#ifdef DEBUG
					cout << "update p2o: " << _pre_id << " " << oidlist_p.size() << endl;
#endif
					sort(oidlist_p.begin(), oidlist_p.end());
					//this->kvstore->updateInsert_p2o(_pre_id, oidlist_p);
					oidlist_p.clear();

#ifdef DEBUG
					cout << "update p2so: " << _pre_id << " " << sidoidlist_p.size() << endl;
#endif
					this->kvstore->updateInsert_p2values(_pre_id, sidoidlist_p);
					sidoidlist_p.clear();
				}
			}
#ifdef DEBUG
		cout << "INSERT PROCESS: OUT p2so..." << endl;
#endif
	}


	for (int i = 0; i < valid_num; ++i)
	{
		delete[] id_tuples[i];
	}
	delete[] id_tuples;

	for (it = old_sigmap.begin(); it != old_sigmap.end(); ++it)
	{
		this->vstree->updateEntry(it->first, it->second);
	}
	for (it = new_sigmap.begin(); it != new_sigmap.end(); ++it)
	{
		SigEntry _sig(it->first, it->second);
		this->vstree->insertEntry(_sig);
	}
#else
	//NOTICE:we deal with insertions one by one here
	//Callers should save the vstree(node and info) after calling this function
	for (int i = 0; i < _triple_num; ++i)
	{
		bool ret = this->insertTriple(_triples[i], &vertices, &predicates);
		if(ret)
		{
			valid_num++;
		}
	}
#endif

	//update string index
	this->stringindex->change(vertices, *this->kvstore, true);
	this->stringindex->change(predicates, *this->kvstore, false);

	return valid_num;
}

int
Database::remove(const TripleWithObjType* _triples, int _triple_num)
{
	vector<int> vertices, predicates;
	int valid_num = 0;

#ifdef USE_GROUP_DELETE
	//NOTICE:this is called by remove(file) or query()(but can not be too large),
	//assume that db is loaded already
	int** id_tuples = new int*[_triple_num];
	int i = 0;
	//for(i = 0; i < _triple_num; ++i)
	//{
	//id_tuples[i] = new int[3];
	//}
	//map<int, EntityBitSet> sigmap;
	//map<int, EntityBitSet>::iterator it;
	EntityBitSet tmpset;
	tmpset.reset();

	int subid, objid, preid;
	bool is_obj_entity;
	for (i = 0; i < _triple_num; ++i)
	{
		string sub = _triples[i].getSubject();
		subid = this->kvstore->getIDByEntity(sub);
		if(subid == -1)
		{
			continue;
		}

		string pre = _triples[i].getPredicate();
		preid = this->kvstore->getIDByPredicate(pre);
		if(preid == -1)
		{
			continue;
		}

		is_obj_entity = _triples[i].isObjEntity();
		string obj = _triples[i].getObject();
		if (is_obj_entity)
		{
			objid = this->kvstore->getIDByEntity(obj);
		}
		else //isObjLiteral
		{
			objid = this->kvstore->getIDByLiteral(obj);
		}
		if(objid == -1)
		{
			continue;
		}

		//if (subid == -1 || preid == -1 || objid == -1)
		//{
			//continue;
		//}
		bool _exist_triple = this->exist_triple(subid, preid, objid);
		if (!_exist_triple)
		{
			continue;
		}

		id_tuples[valid_num] = new int[3];
		id_tuples[valid_num][0] = subid;
		id_tuples[valid_num][1] = preid;
		id_tuples[valid_num][2] = objid;
		this->triples_num--;
		valid_num++;
	}

	//NOTICE:sort and remove duplicates, update the valid num
	//Notice that duplicates in a group can cause problem

	int sub_degree, obj_degree, pre_degree;
	string tmpstr;
	//sort and update kvstore: 11 indexes
	//
	//BETTER:maybe also use int* here with a size to start
	//NOTICE:all kvtrees are opened now, one by one if memory is bottleneck
	//
	//spo cmp: s2p s2o s2po sp2o
	{
#ifdef DEBUG
		cout << "INSRET PROCESS: to spo cmp and update" << endl;
#endif
		qsort(id_tuples, valid_num, sizeof(int*), KVstore::_spo_cmp);
		vector<int> oidlist_s;
		vector<int> pidlist_s;
		vector<int> oidlist_sp;
		vector<int> pidoidlist_s;

		bool _sub_change = true;
		bool _sub_pre_change = true;
		bool _pre_change = true;

		for (int i = 0; i < valid_num; ++i)
			if (i + 1 == valid_num || (id_tuples[i][0] != id_tuples[i + 1][0] || id_tuples[i][1] != id_tuples[i + 1][1] || id_tuples[i][2] != id_tuples[i + 1][2]))
			{
				int _sub_id = id_tuples[i][0];
				int _pre_id = id_tuples[i][1];
				int _obj_id = id_tuples[i][2];

				oidlist_s.push_back(_obj_id);
				oidlist_sp.push_back(_obj_id);
				pidoidlist_s.push_back(_pre_id);
				pidoidlist_s.push_back(_obj_id);
				pidlist_s.push_back(_pre_id);

				_sub_change = (i + 1 == valid_num) || (id_tuples[i][0] != id_tuples[i + 1][0]);
				_pre_change = (i + 1 == valid_num) || (id_tuples[i][1] != id_tuples[i + 1][1]);
				_sub_pre_change = _sub_change || _pre_change;

				if (_sub_pre_change)
				{
					this->kvstore->updateRemove_sp2o(_sub_id, _pre_id, oidlist_sp);
					oidlist_sp.clear();
				}

				if (_sub_change)
				{
					this->kvstore->updateRemove_s2p(_sub_id, pidlist_s);
					pidlist_s.clear();
					this->kvstore->updateRemove_s2po(_sub_id, pidoidlist_s);
					pidoidlist_s.clear();

					sort(oidlist_s.begin(), oidlist_s.end());
					this->kvstore->updateRemove_s2o(_sub_id, oidlist_s);
					oidlist_s.clear();

					sub_degree = (this->kvstore)->getEntityDegree(_sub_id);
					if (sub_degree == 0)
					{
						tmpstr = this->kvstore->getEntityByID(_sub_id);
						this->kvstore->subEntityByID(_sub_id);
						this->kvstore->subIDByEntity(tmpstr);
						(this->vstree)->removeEntry(_sub_id);
						this->freeEntityID(_sub_id);
						this->sub_num--;
						//add info and update buffer
						vertices.push_back(_sub_id);
						if (_sub_id < this->entity_buffer_size)
						{
							this->entity_buffer->del(_sub_id);
						}
					}
					else
					{
						tmpset.reset();
						this->calculateEntityBitSet(_sub_id, tmpset);
						this->vstree->replaceEntry(_sub_id, tmpset);
					}
				}

			}
#ifdef DEBUG
		cout << "INSERT PROCESS: OUT s2po..." << endl;
#endif
	}
	//ops cmp: o2p o2s o2ps op2s
	{
#ifdef DEBUG
		cout << "INSRET PROCESS: to ops cmp and update" << endl;
#endif
		qsort(id_tuples, valid_num, sizeof(int**), KVstore::_ops_cmp);
		vector<int> sidlist_o;
		vector<int> sidlist_op;
		vector<int> pidsidlist_o;
		vector<int> pidlist_o;

		bool _obj_change = true;
		bool _pre_change = true;
		bool _obj_pre_change = true;

		for (int i = 0; i < valid_num; ++i)
			if (i + 1 == valid_num || (id_tuples[i][0] != id_tuples[i + 1][0] || id_tuples[i][1] != id_tuples[i + 1][1] || id_tuples[i][2] != id_tuples[i + 1][2]))
			{
				int _sub_id = id_tuples[i][0];
				int _pre_id = id_tuples[i][1];
				int _obj_id = id_tuples[i][2];

				sidlist_o.push_back(_sub_id);
				sidlist_op.push_back(_sub_id);
				pidsidlist_o.push_back(_pre_id);
				pidsidlist_o.push_back(_sub_id);
				pidlist_o.push_back(_pre_id);

				_obj_change = (i + 1 == valid_num) || (id_tuples[i][2] != id_tuples[i + 1][2]);
				_pre_change = (i + 1 == valid_num) || (id_tuples[i][1] != id_tuples[i + 1][1]);
				_obj_pre_change = _obj_change || _pre_change;

				if (_obj_pre_change)
				{
					this->kvstore->updateRemove_op2s(_obj_id, _pre_id, sidlist_op);
					sidlist_op.clear();
				}

				if (_obj_change)
				{
					sort(sidlist_o.begin(), sidlist_o.end());
					this->kvstore->updateRemove_o2s(_obj_id, sidlist_o);
					sidlist_o.clear();
					this->kvstore->updateRemove_o2ps(_obj_id, pidsidlist_o);
					pidsidlist_o.clear();

					this->kvstore->updateRemove_o2p(_obj_id, pidlist_o);
					pidlist_o.clear();

					is_obj_entity = this->objIDIsEntityID(_obj_id);
					if (is_obj_entity)
					{
						obj_degree = this->kvstore->getEntityDegree(_obj_id);
						if (obj_degree == 0)
						{
							tmpstr = this->kvstore->getEntityByID(_obj_id);
							this->kvstore->subEntityByID(_obj_id);
							this->kvstore->subIDByEntity(tmpstr);
							(this->vstree)->removeEntry(_obj_id);
							this->freeEntityID(_obj_id);
							//add info and update buffer
							vertices.push_back(_obj_id);
							if (_obj_id < this->entity_buffer_size)
							{
								this->entity_buffer->del(_obj_id);
							}
						}
						else
						{
							tmpset.reset();
							this->calculateEntityBitSet(_obj_id, tmpset);
							this->vstree->replaceEntry(_obj_id, tmpset);
						}
					}
					else
					{
						obj_degree = this->kvstore->getLiteralDegree(_obj_id);
						if (obj_degree == 0)
						{
							tmpstr = this->kvstore->getLiteralByID(_obj_id);
							this->kvstore->subLiteralByID(_obj_id);
							this->kvstore->subIDByLiteral(tmpstr);
							this->freeLiteralID(_obj_id);
							//add info and update buffer
							vertices.push_back(_obj_id);
							int tid = _obj_id - Util::LITERAL_FIRST_ID;
							if (tid < this->literal_buffer_size)
							{
								this->literal_buffer->del(tid);
							}
						}
					}
				}

			}
#ifdef DEBUG
		cout << "INSERT PROCESS: OUT o2ps..." << endl;
#endif
	}
	//pso cmp: p2s p2o p2so
	{
#ifdef DEBUG
		cout << "INSRET PROCESS: to pso cmp and update" << endl;
#endif
		qsort(id_tuples, valid_num, sizeof(int*), KVstore::_pso_cmp);
		vector<int> sidlist_p;
		vector<int> oidlist_p;
		vector<int> sidoidlist_p;

		bool _pre_change = true;
		bool _sub_change = true;
		//bool _pre_sub_change = true;

		for (int i = 0; i < valid_num; i++)
			if (i + 1 == valid_num || (id_tuples[i][0] != id_tuples[i + 1][0] || id_tuples[i][1] != id_tuples[i + 1][1] || id_tuples[i][2] != id_tuples[i + 1][2]))
			{
				int _sub_id = id_tuples[i][0];
				int _pre_id = id_tuples[i][1];
				int _obj_id = id_tuples[i][2];

				oidlist_p.push_back(_obj_id);
				sidoidlist_p.push_back(_sub_id);
				sidoidlist_p.push_back(_obj_id);
				sidlist_p.push_back(_sub_id);

				_pre_change = (i + 1 == valid_num) || (id_tuples[i][1] != id_tuples[i + 1][1]);
				_sub_change = (i + 1 == valid_num) || (id_tuples[i][0] != id_tuples[i + 1][0]);
				//_pre_sub_change = _pre_change || _sub_change;

				if (_pre_change)
				{
					this->kvstore->updateRemove_p2s(_pre_id, sidlist_p);
					sidlist_p.clear();

					sort(oidlist_p.begin(), oidlist_p.end());
					this->kvstore->updateRemove_p2o(_pre_id, oidlist_p);
					oidlist_p.clear();

					this->kvstore->updateRemove_p2so(_pre_id, sidoidlist_p);
					sidoidlist_p.clear();

					pre_degree = this->kvstore->getPredicateDegree(_pre_id);
					if (pre_degree == 0)
					{
						tmpstr = this->kvstore->getPredicateByID(_pre_id);
						this->kvstore->subPredicateByID(_pre_id);
						this->kvstore->subIDByPredicate(tmpstr);
						this->freePredicateID(_pre_id);
						//add pre info
						predicates.push_back(_pre_id);
					}
				}
			}
#ifdef DEBUG
		cout << "INSERT PROCESS: OUT p2so..." << endl;
#endif
	}


	for (int i = 0; i < valid_num; ++i)
	{
		delete[] id_tuples[i];
	}
	delete[] id_tuples;
#else
	//NOTICE:we deal with deletions one by one here
	//Callers should save the vstree(node and info) after calling this function
	for (int i = 0; i < _triple_num; ++i)
	{
		bool ret = this->removeTriple(_triples[i], &vertices, &predicates);
		if(ret)
		{
			valid_num++;
		}
	}
#endif

	//update string index
	this->stringindex->disable(vertices, true);
	this->stringindex->disable(predicates, false);

	return valid_num;
}

bool
Database::objIDIsEntityID(int _id)
{
	return _id < Util::LITERAL_FIRST_ID;
}

bool
Database::getFinalResult(SPARQLquery& _sparql_q, ResultSet& _result_set)
{
#ifdef DEBUG_PRECISE
	printf("getFinalResult:begins\n");
#endif
	// this is only selected var num
	int _var_num = _sparql_q.getQueryVarNum();
	_result_set.setVar(_sparql_q.getQueryVar());
	vector<BasicQuery*>& query_vec = _sparql_q.getBasicQueryVec();

	//sum the answer number
	int _ans_num = 0;
#ifdef DEBUG_PRECISE
	printf("getFinalResult:before ansnum loop\n");
#endif
	for (unsigned i = 0; i < query_vec.size(); i++)
	{
		_ans_num += query_vec[i]->getResultList().size();
	}
#ifdef DEBUG_PRECISE
	printf("getFinalResult:after ansnum loop\n");
#endif

	_result_set.ansNum = _ans_num;
#ifndef STREAM_ON
	_result_set.answer = new string*[_ans_num];
	for (int i = 0; i < _result_set.ansNum; i++)
	{
		_result_set.answer[i] = NULL;
	}
#else
	vector<int> keys;
	vector<bool> desc;
	_result_set.openStream(keys, desc);
#ifdef DEBUG_PRECISE
	printf("getFinalResult:after open stream\n");
#endif
#endif
#ifdef DEBUG_PRECISE
	printf("getFinalResult:before main loop\n");
#endif
	int tmp_ans_count = 0;
	//map int ans into string ans
	//union every basic result into total result
	for (unsigned i = 0; i < query_vec.size(); i++)
	{
		vector<int*>& tmp_vec = query_vec[i]->getResultList();
		//ensure the spo order is right, but the triple order is still reversed
		//for every result group in resultlist
		//for(vector<int*>::reverse_iterator itr = tmp_vec.rbegin(); itr != tmp_vec.rend(); ++itr)
		for (vector<int*>::iterator itr = tmp_vec.begin(); itr != tmp_vec.end(); ++itr)
		{
			//to ensure the order so do reversely in two nested loops
#ifndef STREAM_ON
			_result_set.answer[tmp_ans_count] = new string[_var_num];
#endif
#ifdef DEBUG_PRECISE
			printf("getFinalResult:before map loop\n");
#endif
			//NOTICE: in new join method only selec_var_num columns,
			//but before in shenxuchuan's join method, not like this.
			//though there is all graph_var_num columns in result_list,
			//we only consider the former selected vars
			//map every ans_id into ans_str
			for (int v = 0; v < _var_num; ++v)
			{
				int ans_id = (*itr)[v];
				string ans_str;
				if (this->objIDIsEntityID(ans_id))
				{
					ans_str = (this->kvstore)->getEntityByID(ans_id);
				}
				else
				{
					ans_str = (this->kvstore)->getLiteralByID(ans_id);
				}
#ifndef STREAM_ON
				_result_set.answer[tmp_ans_count][v] = ans_str;
#else
				_result_set.writeToStream(ans_str);
#endif
#ifdef DEBUG_PRECISE
				printf("getFinalResult:after copy/write\n");
#endif
			}
			tmp_ans_count++;
		}
	}
#ifdef STREAM_ON
	_result_set.resetStream();
#endif
#ifdef DEBUG_PRECISE
	printf("getFinalResult:ends\n");
#endif

	return true;
}

void
Database::printIDlist(int _i, int* _list, int _len, string _log)
{
	stringstream _ss;
	_ss << "[" << _i << "] ";
	for (int i = 0; i < _len; i++) {
		_ss << _list[i] << "\t";
	}
	Util::logging("==" + _log + ":");
	Util::logging(_ss.str());
}

void
Database::printPairList(int _i, int* _list, int _len, string _log)
{
	stringstream _ss;
	_ss << "[" << _i << "] ";
	for (int i = 0; i < _len; i += 2) {
		_ss << "[" << _list[i] << "," << _list[i + 1] << "]\t";
	}
	Util::logging("==" + _log + ":");
	Util::logging(_ss.str());
}

void
Database::test()
{
	int subNum = 9, preNum = 20, objNum = 90;

	int* _id_list = NULL;
	int _list_len = 0;
	{   /* x2ylist */
		for (int i = 0; i < subNum; i++)
		{

			(this->kvstore)->getobjIDlistBysubID(i, _id_list, _list_len);
			if (_list_len != 0)
			{
				stringstream _ss;
				this->printIDlist(i, _id_list, _list_len, "s2olist[" + _ss.str() + "]");
				delete[] _id_list;
			}

			/* o2slist */
			(this->kvstore)->getsubIDlistByobjID(i, _id_list, _list_len);
			if (_list_len != 0)
			{
				stringstream _ss;
				this->printIDlist(i, _id_list, _list_len, "o(sub)2slist[" + _ss.str() + "]");
				delete[] _id_list;
			}
		}

		for (int i = 0; i < objNum; i++)
		{
			int _i = Util::LITERAL_FIRST_ID + i;
			(this->kvstore)->getsubIDlistByobjID(_i, _id_list, _list_len);
			if (_list_len != 0)
			{
				stringstream _ss;
				this->printIDlist(_i, _id_list, _list_len, "o(literal)2slist[" + _ss.str() + "]");
				delete[] _id_list;
			}
		}
	}
	{   /* xy2zlist */
		for (int i = 0; i < subNum; i++)
		{
			for (int j = 0; j < preNum; j++)
			{
				(this->kvstore)->getobjIDlistBysubIDpreID(i, j, _id_list,
					_list_len);
				if (_list_len != 0)
				{
					stringstream _ss;
					_ss << "preid:" << j;
					this->printIDlist(i, _id_list, _list_len, "sp2olist[" + _ss.str() + "]");
					delete[] _id_list;
				}

				(this->kvstore)->getsubIDlistByobjIDpreID(i, j, _id_list,
					_list_len);
				if (_list_len != 0)
				{
					stringstream _ss;
					_ss << "preid:" << j;
					this->printIDlist(i, _id_list, _list_len, "o(sub)p2slist[" + _ss.str() + "]");
					delete[] _id_list;
				}
			}
		}

		for (int i = 0; i < objNum; i++)
		{
			int _i = Util::LITERAL_FIRST_ID + i;
			for (int j = 0; j < preNum; j++)
			{
				(this->kvstore)->getsubIDlistByobjIDpreID(_i, j, _id_list,
					_list_len);
				if (_list_len != 0)
				{
					stringstream _ss;
					_ss << "preid:" << j;
					this->printIDlist(_i, _id_list, _list_len,
						"*o(literal)p2slist[" + _ss.str() + "]");
					delete[] _id_list;
				}
			}
		}
	}
	{   /* x2yzlist */
		for (int i = 0; i < subNum; i++)
		{
			(this->kvstore)->getpreIDobjIDlistBysubID(i, _id_list, _list_len);
			if (_list_len != 0)
			{
				this->printPairList(i, _id_list, _list_len, "s2polist");
				delete[] _id_list;
				_list_len = 0;
			}
		}

		for (int i = 0; i < subNum; i++)
		{
			(this->kvstore)->getpreIDsubIDlistByobjID(i, _id_list, _list_len);
			if (_list_len != 0)
			{
				this->printPairList(i, _id_list, _list_len, "o(sub)2pslist");
				delete[] _id_list;
			}
		}

		for (int i = 0; i < objNum; i++)
		{
			int _i = Util::LITERAL_FIRST_ID + i;
			(this->kvstore)->getpreIDsubIDlistByobjID(_i, _id_list, _list_len);
			if (_list_len != 0)
			{
				this->printPairList(_i, _id_list, _list_len,
					"o(literal)2pslist");
				delete[] _id_list;
			}
		}
	}
}

void
Database::test_build_sig()
{
	BasicQuery* _bq = new BasicQuery("");
	/*
	* <!!!>	y:created	<!!!_(album)>.
	*  <!!!>	y:created	<Louden_Up_Now>.
	*  <!!!_(album)>	y:hasSuccessor	<Louden_Up_Now>
	* <!!!_(album)>	rdf:type	<wordnet_album_106591815>
	*
	* id of <!!!> is 0
	* id of <!!!_(album)> is 2
	*
	*
	* ?x1	y:created	?x2.
	*  ?x1	y:created	<Louden_Up_Now>.
	*  ?x2	y:hasSuccessor	<Louden_Up_Now>.
	* ?x2	rdf:type	<wordnet_album_106591815>
	*/
	{
		Triple _triple("?x1", "y:created", "?x2");
		_bq->addTriple(_triple);
	}
	{
		Triple _triple("?x1", "y:created", "<Louden_Up_Now>");
		_bq->addTriple(_triple);
	}
	{
		Triple _triple("?x2", "y:hasSuccessor", "<Louden_Up_Now>");
		_bq->addTriple(_triple);
	}
	{
		Triple _triple("?x2", "rdf:type", "<wordnet_album_106591815>");
		_bq->addTriple(_triple);
	}
	vector<string> _v;
	_v.push_back("?x1");
	_v.push_back("?x2");

	_bq->encodeBasicQuery(this->kvstore, _v);
	Util::logging(_bq->to_str());
	SPARQLquery _q;
	_q.addBasicQuery(_bq);

	(this->vstree)->retrieve(_q);

	Util::logging("\n\n");
	Util::logging("candidate:\n\n" + _q.candidate_str());
}

//void
//Database::test_join()
//{
//BasicQuery* _bq = new BasicQuery("");
//
//* <!!!>	y:created	<!!!_(album)>.
//*  <!!!>	y:created	<Louden_Up_Now>.
//*  <!!!_(album)>	y:hasSuccessor	<Louden_Up_Now>
//* <!!!_(album)>	rdf:type	<wordnet_album_106591815>
//*
//* id of <!!!> is 0
//* id of <!!!_(album)> is 2
//*
//*
//* ?x1	y:created	?x2.
//*  ?x1	y:created	<Louden_Up_Now>.
//*  ?x2	y:hasSuccessor	<Louden_Up_Now>.
//* ?x2	rdf:type	<wordnet_album_106591815>
//
//{
////Triple _triple("?x1", "y:created", "?x2");
//_bq->addTriple(_triple);
//}
//{
//Triple _triple("?x1", "y:created", "<Louden_Up_Now>");
//_bq->addTriple(_triple);
//}
//{
//Triple _triple("?x2", "y:hasSuccessor", "<Louden_Up_Now>");
//_bq->addTriple(_triple);
//}
//{
//Triple _triple("?x2", "rdf:type", "<wordnet_album_106591815>");
//_bq->addTriple(_triple);
//}
//vector<string> _v;
//_v.push_back("?x1");
//_v.push_back("?x2");
//_bq->encodeBasicQuery(this->kvstore, _v);
//Util::logging(_bq->to_str());
//SPARQLquery _q;
//_q.addBasicQuery(_bq);
//(this->vstree)->retrieve(_q);
//Util::logging("\n\n");
//Util::logging("candidate:\n\n"+_q.candidate_str());
//_q.print(cout);
//this->join(_q);
//ResultSet _rs;
//this->getFinalResult(_q, _rs);
//cout << _rs.to_str() << endl;
//}
