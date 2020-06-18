#include"commandFactory.hpp"

CommandFactory::CommandFactory()
{
	addCommand();
}

ICommand *CommandFactory::getCommandProcesser(const char *pCmd)
{
	ICommand *pProcessor=NULL;
	//在map中通过pCmd寻找对应的命令并返回
	do{
		COMMAND_MAP::iterator iter;
		iter=_cmdMap.find(pCmd);
		if(iter!=_cmdMap.end())
		{
			pProcessor=iter->second;
		}
	}while(0);
	return pProcessor;
}
