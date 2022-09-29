#include "sqlconnpool.h"

using namespace std;

connection_pool::connection_pool()
{
	m_CurConn = 0;
	m_FreeConn = 0;
}

connection_pool* connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string id, string name, string passwd, string DBName, int Port, int MaxConn)
{
    m_id = id;
	m_Port = Port;
	m_name = name;
	m_passwd = passwd;
	m_DatabaseName = DBName;
	
	
	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			Log::getInstance()->writelog(3, "localhost", "MySQL Error\n");
			exit(1);
		}
		con = mysql_real_connect(con, id.c_str(), m_name.c_str(), m_passwd.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			Log::getInstance()->writelog(3, "localhost", "MySQL Error\n");
			exit(1);
		}
		connList.push_back(con);
		m_FreeConn++;
	}

	sem_init(&m_sem, 0, m_FreeConn);

	m_MaxConn = m_FreeConn;
}

string connection_pool::retquery(MYSQL* con, int id) {		//返回查询id对应的学生信息
	MYSQL_RES* sel_res;	//返回行的查询结果
	MYSQL_ROW sel_row;	//一行数据的“类型安全”表示，通过调用mysql_fetch_row()从MYSQL_RES变量获得
	int res_num;		//存放结果集列数
	string ret = "";
	//查询对应的所有信息
	string check = "SELECT * FROM student WHERE id = " + to_string(id);
    char* p = &check[0];
	if (mysql_query(con, const_cast<const char*>(p)))	//若查询失败
		{ 
			cout << "查询失败" << endl;
			return NULL;
		}
	else {
		sel_res = mysql_store_result(con);		//查询到的结果保存到sel_res中
		
		res_num = mysql_num_fields(sel_res);	//把结果集列数存放到num中

		while ((sel_row = mysql_fetch_row(sel_res)))	//遇到最后一行就中止
		{
			for (int i = 0; i < res_num; i++)	//输出该行每一列
			{
                ret += static_cast<string>(sel_row[i]);
                ret += ";";
            }
		}
		mysql_free_result(sel_res);			//释放结果集所占用的内存
		return ret;
    }
}
//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	
	MYSQL *con = NULL;

	if (connList.size() == 0 )
		return NULL;

	sem_wait(&m_sem);
	
	{
        lock_guard<mutex>locker(m_mutex);
        con = connList.front();
        connList.pop_front();

        m_FreeConn--;
        m_CurConn++;
    }
	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (con == nullptr)
		return false;

    {
        lock_guard<mutex>locker(m_mutex);
        connList.push_back(con);
        m_FreeConn++;
        m_CurConn--;
    }
    
	sem_post(&m_sem);
	return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{
    lock_guard<mutex>locker(m_mutex);
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
	DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	
	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}