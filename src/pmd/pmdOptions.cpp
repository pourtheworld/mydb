#include"pmdOptions.hpp"
#include"pd.hpp"

//构造，初始化内存等
pmdOptions::pmdOptions()
{
	memset(_dbPath,0,sizeof(_dbPath));
	memset(_logPath,0,sizeof(_logPath));
	memset(_confPath,0,sizeof(_confPath));
	memset(_svcName,0,sizeof(_svcName));
	_maxPool=NUMPOOL;
}

pmdOptions::~pmdOptions(){}

//用户命令行自定义添加选项argc及对应值argv
//传出修改过后的vm
int pmdOptions::readCmd(int argc,char **argv,po::options_description &desc,po::variables_map &vm)
{
	int rc=EDB_OK;
	try
	{	//将argc,argv放到cmd的编译器中，根据description判断是否能添加进vm
		po::store(po::command_line_parser(argc,argv).options(desc).allow_unregistered().run(),vm);
		//通知vm
		po::notify(vm);
	}
	catch(po::unknown_option &e)
	{	//未知的选项变量
		std::cerr<<"Invalid arguments: "<<e.get_option_name()<<std::endl;
		rc=EDB_INVALIDARG;
		goto error;
	}
	catch(po::error &e)
	{	//错误的选项变量
		std::cerr<<"Error: "<<e.what()<<std::endl;
		rc=EDB_INVALIDARG;
		goto error;
	}
done:
	return rc;
error:
	goto done;
}

int pmdOptions::importVM(const po::variables_map &vm,bool isDefault)
{
	int rc=EDB_OK;
	const char *p=NULL;
	
	//配置文件路径
	//PMD_OPTION_CONPATH值不为0，将值赋给_confPath
	if(vm.count(PMD_OPTION_CONFPATH))
	{
		p=vm[PMD_OPTION_CONFPATH].as<string>().c_str();
		strncpy(_confPath,p,OSS_MAX_PATHSIZE);
	}//否则将当前值赋给它
	else if(isDefault)	strcpy(_confPath,"./"CONFFILENAME);

	//日志文件路径
	if(vm.count(PMD_OPTION_LOGPATH))
	{
		p=vm[PMD_OPTION_LOGPATH].as<string>().c_str();
		strncpy(_logPath,p,OSS_MAX_PATHSIZE);
	}
	else if(isDefault)
	{
		strcpy(_logPath,"./"LOGFILENAME);
	}
	
	//数据库文件路径
	if(vm.count(PMD_OPTION_DBPATH))
	{
		p=vm[PMD_OPTION_DBPATH].as<string>().c_str();
		strncpy(_dbPath,p,OSS_MAX_PATHSIZE);
	}
	else if(isDefault)
	{
		strcpy(_dbPath,"./"DBFILENAME);
	}

	//服务名
	 if ( vm.count ( PMD_OPTION_SVCNAME ) )
  	 {
     		 p = vm[PMD_OPTION_SVCNAME].as<string>().c_str() ;
      		strncpy ( _svcName, p, NI_MAXSERV ) ;
  	 }
  	 else if ( isDefault )
  	 {
     		 strcpy ( _svcName, SVCNAME ) ;
  	 }

  	 // maxpool
  	 if ( vm.count ( PMD_OPTION_MAXPOOL ) )
  	 {
     		 _maxPool = vm [ PMD_OPTION_MAXPOOL ].as<unsigned int> () ;
  	 }
  	 else if ( isDefault )
  	 {
     		 _maxPool = NUMPOOL ;
  	 }
  	 return rc ;
}

//从配置文件中读取配置选项
int pmdOptions::readConfigureFile ( const char *path,
                                    po::options_description &desc,
                                    po::variables_map &vm )
{
   int rc                        = EDB_OK ;
   char conf[OSS_MAX_PATHSIZE+1] = {0} ;
   //conf从给定的Path中获取配置文件路径
   strncpy ( conf, path, OSS_MAX_PATHSIZE ) ;
   try
   {	//从配置文件中分析是否符合description的要求
      po::store ( po::parse_config_file<char> ( conf, desc, true ), vm ) ;
      po::notify ( vm ) ;
   }
   catch( po::reading_file )
   {
      std::cerr << "Failed to open config file: "
                <<( std::string ) conf << std::endl
                << "Using default settings" << std::endl ;
      rc = EDB_IO ;
      goto error ;
   }
   catch ( po::unknown_option &e )
   {
      std::cerr << "Unkown config element: "
                << e.get_option_name () << std::endl ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   catch ( po::invalid_option_value &e )
   {
      std::cerr << ( std::string ) "Invalid config element: "
                << e.get_option_name () << std::endl ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   catch( po::error &e )
   {
      std::cerr << e.what () << std::endl ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
done :
   return rc ;
error :
   goto done ;
}

int pmdOptions::init( int argc, char **argv )
{
   int rc = EDB_OK ;
   //等待赋值的description
   po::options_description all ( "Command options" ) ;
	
   //用户自定义配置选项vm
   po::variables_map vm ;

   //配置文件读取的vm2
   po::variables_map vm2 ;
  
   //自定义description赋值宏
   PMD_ADD_PARAM_OPTIONS_BEGIN( all )
      PMD_COMMANDS_OPTIONS
   PMD_ADD_PARAM_OPTIONS_END
   
   //先从用户自定义cmd中获取配置选项给到vm
   rc = readCmd ( argc, argv, all, vm ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to read cmd, rc = %d", rc ) ;
      goto error ;
   }
   //如果含有Help的选项，那么将description打印到屏幕上结束
   if ( vm.count ( PMD_OPTION_HELP ) )
   {
      std::cout << all << std::endl ;
      rc = EDB_PMD_HELP_ONLY ;
      goto done ;
   }
   // 如果用户的vm中存在配置文件路径，那么读取配置文件到vm2
   if ( vm.count ( PMD_OPTION_CONFPATH ) )
   {
      rc = readConfigureFile ( vm[PMD_OPTION_CONFPATH].as<string>().c_str(),
                               all, vm2 ) ;
   }
   if ( rc )
   {
      PD_LOG ( PDERROR, "Unexpected error when reading conf file, rc = %d",
               rc ) ;
      goto error ;
   }
   // load vm from file
   rc = importVM ( vm2 ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to import from vm2, rc = %d", rc ) ;
      goto error ;
   }
   // load vm from command line
   rc = importVM ( vm ) ;
   if ( rc )
   {
      PD_LOG ( PDERROR, "Failed to import from vm, rc = %d", rc ) ;
      goto error ;
   }
done :
   return rc ;
error :
   goto done ;
}
