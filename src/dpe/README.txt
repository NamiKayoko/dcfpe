1. ������
�ο�main.cc��
��֤dpe.h��ͬһĿ¼�¡�

2. ����Binary (32bit,64bit����)
������MinGW��VC

MinGW:
g++ main.cc --std=c++11 -O3

3. ���� 
��֤��ͬһĿ¼���������ļ�:
a.exe 2�������binary
dpe.dll dpe֧��dll (dpeʵ��)
zmq.dll zmq֧��dll (��Ϣ��)
msvcp120.dll, msvcr120.dll C++����ʱ��
pm.exe ProcessMonitor,��ѡ,���ڼ�ؽ���,�ڱ���ؽ����˳�ʱ����

��Master(���ΪServer)��Worker����ϵĲ��𷽷���ͬ

4. �����вο�
��ʽ: --key=value�� -key value
(-�ĸ�����Ӱ������,key�����ִ�Сд)

4.1 a.exe [ѡ���б�]

--l=logLevel
--log=logLevel
��־����,һ��ֵȡ0��1.
Ĭ�ϼ���1.

--t=type
--type=type
�������,ֵΪserver��worker.
Ĭ��ֵserver.

--ip=ip
������ip.
Ĭ��ֵΪ���ص�һ�����õ�ip.

--server_ip=server_ip
Master����ip.
��ѡ���Master�����Ч.

--p=port
--port=port
�����Ķ˿�.
serverĬ��ֵΪ3310
workerĬ��ֵΪ3311

--id=id
ʵ��id.
ֻ��worker�����Ч.
Ĭ��ֵ0.
���ڱ�ʶ��һ����ϲ�ͬ��instance.
δʹ��port����ʱ,Ĭ�϶˿�Ϊ Ĭ��ֵ+id

ʾ��:
��ͬһ̨�������

Master���:
a.exe --l=0

Worker���:

����1��worker���
a.exe --l=0 -type=worker

����2��worker���
a.exe --l=0 -type=worker --id=0
a.exe --l=0 -type=worker --id=1

�ڲ�ͬ�������

Master���:
a.exe --l=0

Worker���:
����1��worker���
a.exe --l=0 -type=worker --server_ip=<server ip>

����2��worker���
a.exe --l=0 -type=worker --id=0 --server_ip=<server ip>
a.exe --l=0 -type=worker --id=1 --server_ip=<server ip>

4.2 pm.exe Ŀ�����·�� [ѡ���б�] -- [�ӽ���ѡ��]

--l=logLevel
--log=logLevel
��־����,һ��ֵȡ0��1.
Ĭ�ϼ���1.

--id=id
--id=minId-maxId
�ӽ��̵�id,��a.exe��ѡ���б�.
--id=id������minId=id,maxId=id
Ĭ��ֵminId=0,maxId=0
��ѡ������ӽ�����Ŀ.

���� Ŀ�����·�� �� ѡ���б��˳���������

4.3 ����

4.3.1 ��һ̨������ϲ���1��master,2��worker.
4.3.1.1
main.exe --log=0
4.3.1.2
pm.exe main.exe --log=0 --id=0-1 -- --log=0 --type=worker

4.3.2 ��һ̨���������1��master,����һ̨������ϲ���2��worker
4.3.2.1
main.exe --log=0
4.3.2.2
pm.exe main.exe --log=0 --id=0-1 -- --log=0 --type=worker --server_ip=192.168.137.128