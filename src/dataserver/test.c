#include <stdio.h>
#include "dataserver.h"
#include <pthread.h>
int main()
{
	struct data_server first;
	int ret;
	printf("%d\n", 2%3);
	printf("%lu\n", sizeof(super_block));
	ret = get_current_imformation(&first);
	if(ret == 1)
	{
		perror("something wrong");
	}
	printf("memory_used is %dMB, free is %dMB\n", first.memory_used>>10, first.memory_free>>10);
	return 0;
}
