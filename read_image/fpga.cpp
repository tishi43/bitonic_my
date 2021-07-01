
#include <random>
#include <stdio.h>




//假定fpga的reg大小16，数据存放在bram，bram 1项存2个数据
//前4 step所有数据可以放进reg中进行

//16个数据，最多完成3轮,这个函数处理1个step，
//前4个step可以在reg中一次性完成，也调这个函数，每个step调1次，
//groups，16个数据处于几个group中
void bitonic_16data(unsigned int *data,int rounds,int step,int ascend_in, int groups)
{

	int j;//遍历round
	int k; //遍历group
	int l; //遍历group里每对数据
	int ii;

	int pairs;         //一组有几对数据
	int M;             //一组有几个数据,M=2*pairs
	int ascend;        //1=升序 0=降序


	for (j = 0; j < rounds; j++){
		M = 16 / groups; //这里的group和verilog的group,sub group不一样，这里group是16个数据来自几个group
		pairs = M / 2;
		for (k = 0; k < groups; k++){
			//round 1, k第0bit决定升降
			//round 2, k第1bit决定升降
			//...
			ascend = step<4?((k >> j) & 0x1) == 0:ascend_in;
			for (l = 0; l < pairs; l++){
				int swap = ascend ? data[k*M + l] >= data[k*M + M / 2 + l] :
					data[k*M + l] <= data[k*M + M / 2 + l];
				if (swap)
				{
					int temp = data[k*M + l];
					data[k*M + l] = data[k*M + M / 2 + l];
					data[k*M + M / 2 + l] = temp;
				}



			}
		}
		groups *= 2;

	}

}

void bitonic_fpga(unsigned int *data, int N)
{
	int i;//遍历step
	int ii,jj;
	unsigned int buf[16];
	int steps = log2(N);
	int N_div16 = N / 16;
	int rounds;
	int j,k,l;
	int interval;
	int groups;
	int max_group_size;      //round的最大组长度
	int step_max_group_size; //step最大的组长度，
	int current_group_size;  //当前size，累积到step_max_group_size重置0
	int ascend;
	int rounds_remain;
	int current_rounds;

	for (j = 0; j < N_div16; j++){
		memcpy(buf, data + j * 16, 64);
		for (i = 1; i <= 4; i++){ //前4步，每16个1组，4步全部完成
			bitonic_16data(buf, i,i,(j&0x1)==0,1<<(4-i));

			printf("16group %3d step %2d: ",j, i);
			for (ii = 0; ii < 16; ii++){
				printf("%4d ", buf[ii]);
			}
			printf("\n");

		}
		memcpy(data + j * 16, buf,64);
	}

	//N=256为例,最后一个step，step 8
	//0,1  32,33   64,65   96,97   128,129   160,161   192,193    224,225
	//round 1    0,1比128,129   32,33比160,161  64,65比192,193   96,97比224,225
	//round 2    0,1比64,65     32,33比96,97    128,129比192,193  160,161比224,225
	//round 3    0,1比32,33     64,65比96,97    128,129比160,161  192,193比224,225
	//round 4    0,1不能比较

    //   3个round之后，最大的组从256变成256/8=32，32buf放不下，间隔2来拷
	//round 4
	//round 5
	//round 6

	// 再3个round之后，最大组变成4,最大组小于等于16之后，都是16，16个数据顺序拷
	//round 7
	//round 8

	//step=7
	// round 1   0,1   16,17  32,33  48,49  64,65   80,81  96,97   112,113    正
	//           128,129 ...                                                  逆

	//step=4之后，step_max_group_size都大于16，bitonic_16data内部全部正或者全部逆，根据step_max_group正逆交替，从外面函数传进去
	//step=1,2,3，符合((k >> j) & 0x1) == 0的规则

	//bitonic_16data需要输出round3,round4的结果，外面判断选择round3的结果还是round4的结果
	//4级result可能会逻辑级数太多，round3_result为reg，round4_result为wire
	for (i = 5; i <= steps; i++){
		rounds = i;
		step_max_group_size = N / (1 <<  (steps-i));

		for (k = 0; k<rounds; k += current_rounds){
			current_group_size = 0;
			ascend = 1;

			max_group_size = N/(1<<(k+steps-i)); //step=7,一上来round=1最大的组是128
			//如上图round 7,实际group_size=4,max_group_size=16
			//bitonic_16data内部是根据传进去的rounds来判断实际的group大小
			if (max_group_size <= 16)
				max_group_size = 16;
			groups = N / max_group_size;
			for (ii = 0; ii < groups; ii++){ //如上面N=256的例子，round4，最大组变成32，这样的大组8组
				for (j = 0; j < max_group_size/16; j++){ //max_group_size内以16为一组分成多少组
					interval = max_group_size<=16?2:max_group_size/8;//间隔多少拷贝数据，每多少数据里面拷2个数据
					for (l = 0; l<8; l++){
						//每次拷2个int
						//j*2:从1,2开始拷，还是3,4

						memcpy(buf + l * 2, data +ii*max_group_size+ j * 2 + l*interval, 8);
					}

					//round还剩4轮以上，16个数据都是从1个组中取出来的
					//round剩1，8组，round剩2，4组
					rounds_remain = rounds - k;
					current_rounds = rounds_remain>4 ? 3 : rounds_remain;
					bitonic_16data(buf, current_rounds, i, ascend,rounds_remain>=4?1:(1<<(4-rounds_remain))); //最后还剩4 round，处理4 round，
					for (l = 0; l < 8; l++){
						memcpy(data +ii*max_group_size+ j * 2 + l*interval, buf + l * 2, 8);
					}

				}
				current_group_size += max_group_size;
				if (current_group_size == step_max_group_size){
					current_group_size = 0;
					ascend = !ascend;
				}
			}

			printf("step %2d round %2d: ", i, k+4>=i?i:k+3);
			for (ii = 0; ii < N; ii++){
				printf("%4d ", data[ii]);
			}
			printf("\n");


		}
	}
}

