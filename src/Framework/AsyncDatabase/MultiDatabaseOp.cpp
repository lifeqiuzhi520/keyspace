#include "MultiDatabaseOp.h"
#include "System/Buffer.h"
#include "System/ThreadPool.h"
#include "System/IO/IOProcessor.h"
#include "Framework/Database/Table.h"
#include "Framework/Database/Transaction.h"

MultiDatabaseOp::MultiDatabaseOp() :
operation(this, &MultiDatabaseOp::Operation),
onComplete(this, &MultiDatabaseOp::OnComplete),
active(false)
{
	Init();
}


void MultiDatabaseOp::Init()
{
	numop = 0;
	//userCallback = NULL;
	tx = NULL;
}


bool MultiDatabaseOp::Get(Table* table, const ByteString &key, ByteString& value)
{
	DatabaseOp* op;
	
	if (numop >= SIZE(ops))
		return false;
	
	op = &op[numop];
	op->op = DatabaseOp::GET;
	op->table = table;
	op->key = key;
	op->value = value;
	
	numop++;
	
	return true;
}


bool MultiDatabaseOp::Put(Table* table, const ByteString& key, ByteString& value)
{
	DatabaseOp* op;

	if (numop >= SIZE(ops))
		return false;
	
	op = &ops[numop];
	op->op = DatabaseOp::PUT;
	op->table = table;
	op->key = key;
	op->value = value;
	
	numop++;
	
	return true;
}

bool MultiDatabaseOp::Put(Table* table, char* key, ByteString &value)
{
	int len;
	
	len = strlen(key);
	const ByteString bsKey(len, len, key);
	
	return MultiDatabaseOp::Put(table, bsKey, value);
}

bool MultiDatabaseOp::Put(Table* table, char* key, char* value)
{
	int len;
	
	len = strlen(key);
	ByteString bsKey(len, len, key);
	
	len = strlen(value);
	ByteString bsValue(len, len, value);
	
	return MultiDatabaseOp::Put(table, bsKey, bsValue);

}

void MultiDatabaseOp::SetTransaction(Transaction* tx)
{
	this->tx = tx;
}

void MultiDatabaseOp::SetCallback(Callable* userCallback_)
{
	this->userCallback = userCallback_;
}

Callable* MultiDatabaseOp::GetOperation()
{
	return &operation;
}

const ByteString* MultiDatabaseOp::GetKey(int i)
{
	return &ops[i].key;
}


ByteString*	MultiDatabaseOp::GetValue(int i)
{
	return &ops[i].value;
}


bool MultiDatabaseOp::GetReturnValue(int i)
{
	return ops[i].ret;
}


int MultiDatabaseOp::GetNumOp()
{
	return numop;
}


void MultiDatabaseOp::Operation()
{
	int			i;
	DatabaseOp* op;
	
	if (tx)
		tx->Begin();
		
	for (i = 0; i < numop; i++)
	{
		op = &ops[i];
		if (op->op == DatabaseOp::GET)
			op->ret = op->table->Get(tx, op->key, op->value);
		else if (op->op == DatabaseOp::PUT)
			op->ret = op->table->Put(tx, op->key, op->value);
	}
	
	if (tx)
		tx->Commit();
		
	IOProcessor::Complete(&onComplete);
}

void MultiDatabaseOp::OnComplete()
{
	active = false;
	
	Call(userCallback);
}