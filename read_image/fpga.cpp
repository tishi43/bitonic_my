
#include <random>
#include <stdio.h>




//�ٶ�fpga��reg��С16�����ݴ����bram��bram 1���2������
//ǰ4 step�������ݿ��ԷŽ�reg�н���

//16�����ݣ�������3��,�����������1��step��
//ǰ4��step������reg��һ������ɣ�Ҳ�����������ÿ��step��1�Σ�
//groups��16�����ݴ��ڼ���group��
void bitonic_16data(unsigned int *data,int rounds,int step,int ascend_in, int groups)
{

	int j;//����round
	int k; //����group
	int l; //����group��ÿ������
	int ii;

	int pairs;         //һ���м�������
	int M;             //һ���м�������,M=2*pairs
	int ascend;        //1=���� 0=����


	for (j = 0; j < rounds; j++){
		M = 16 / groups; //�����group��verilog��group,sub group��һ��������group��16���������Լ���group
		pairs = M / 2;
		for (k = 0; k < groups; k++){
			//round 1, k��0bit��������
			//round 2, k��1bit��������
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
	int i;//����step
	int ii,jj;
	unsigned int buf[16];
	int steps = log2(N);
	int N_div16 = N / 16;
	int rounds;
	int j,k,l;
	int interval;
	int groups;
	int max_group_size;      //round������鳤��
	int step_max_group_size; //step�����鳤�ȣ�
	int current_group_size;  //��ǰsize���ۻ���step_max_group_size����0
	int ascend;
	int rounds_remain;
	int current_rounds;

	for (j = 0; j < N_div16; j++){
		memcpy(buf, data + j * 16, 64);
		for (i = 1; i <= 4; i++){ //ǰ4����ÿ16��1�飬4��ȫ�����
			bitonic_16data(buf, i,i,(j&0x1)==0,1<<(4-i));

			printf("16group %3d step %2d: ",j, i);
			for (ii = 0; ii < 16; ii++){
				printf("%4d ", buf[ii]);
			}
			printf("\n");

		}
		memcpy(data + j * 16, buf,64);
	}

	//N=256Ϊ��,���һ��step��step 8
	//0,1  32,33   64,65   96,97   128,129   160,161   192,193    224,225
	//round 1    0,1��128,129   32,33��160,161  64,65��192,193   96,97��224,225
	//round 2    0,1��64,65     32,33��96,97    128,129��192,193  160,161��224,225
	//round 3    0,1��32,33     64,65��96,97    128,129��160,161  192,193��224,225
	//round 4    0,1���ܱȽ�

    //   3��round֮���������256���256/8=32��32buf�Ų��£����2����
	//round 4
	//round 5
	//round 6

	// ��3��round֮���������4,�����С�ڵ���16֮�󣬶���16��16������˳��
	//round 7
	//round 8

	//step=7
	// round 1   0,1   16,17  32,33  48,49  64,65   80,81  96,97   112,113    ��
	//           128,129 ...                                                  ��

	//step=4֮��step_max_group_size������16��bitonic_16data�ڲ�ȫ��������ȫ���棬����step_max_group���潻�棬�����溯������ȥ
	//step=1,2,3������((k >> j) & 0x1) == 0�Ĺ���

	//bitonic_16data��Ҫ���round3,round4�Ľ���������ж�ѡ��round3�Ľ������round4�Ľ��
	//4��result���ܻ��߼�����̫�࣬round3_resultΪreg��round4_resultΪwire
	for (i = 5; i <= steps; i++){
		rounds = i;
		step_max_group_size = N / (1 <<  (steps-i));

		for (k = 0; k<rounds; k += current_rounds){
			current_group_size = 0;
			ascend = 1;

			max_group_size = N/(1<<(k+steps-i)); //step=7,һ����round=1��������128
			//����ͼround 7,ʵ��group_size=4,max_group_size=16
			//bitonic_16data�ڲ��Ǹ��ݴ���ȥ��rounds���ж�ʵ�ʵ�group��С
			if (max_group_size <= 16)
				max_group_size = 16;
			groups = N / max_group_size;
			for (ii = 0; ii < groups; ii++){ //������N=256�����ӣ�round4���������32�������Ĵ���8��
				for (j = 0; j < max_group_size/16; j++){ //max_group_size����16Ϊһ��ֳɶ�����
					interval = max_group_size<=16?2:max_group_size/8;//������ٿ������ݣ�ÿ�����������濽2������
					for (l = 0; l<8; l++){
						//ÿ�ο�2��int
						//j*2:��1,2��ʼ��������3,4

						memcpy(buf + l * 2, data +ii*max_group_size+ j * 2 + l*interval, 8);
					}

					//round��ʣ4�����ϣ�16�����ݶ��Ǵ�1������ȡ������
					//roundʣ1��8�飬roundʣ2��4��
					rounds_remain = rounds - k;
					current_rounds = rounds_remain>4 ? 3 : rounds_remain;
					bitonic_16data(buf, current_rounds, i, ascend,rounds_remain>=4?1:(1<<(4-rounds_remain))); //���ʣ4 round������4 round��
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

