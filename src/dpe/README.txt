����·��:https://github.com/baihacker/dcfpe/blob/master/src/dpe/README.txt
��ͨ�������ӷ������°汾.

1. ������
�ο�main.cc.
��֤����main.ccʱ���������õ�dpe.h:
dpe.h�ں�main.ccͬһĿ¼�»��������include·����.

2. ����Ӧ�ó���Binary (32bit,64bit����)
MinGW:
g++ main.cc --std=c++11 -O3

ͨ��CPP_INCLUDE_PATH�����������ñ�������include·��.

VC:
�����������ò�����·�������,��ʹ��vc�����б���.

sb:
https://github.com/baihacker/sb
ͨ������compilers.json������pe++.py����MinGW�Լ�vc++.py����vc���б���

3. ���� 
��֤��ͬһĿ¼���������ļ�:
a.exe 2�������binary,demo��Ϊmain.exe
dpe.dll dpe֧��dll (dpeʵ��)
zmq.dll zmq֧��dll (��Ϣ��)
msvcp120.dll, msvcr120.dll C++����ʱ��
pm.exe ��ѡ��ProcessMonitor,����ͬʱ��������ӽ���,����ӽ���,�ڱ���ؽ����˳�ʱ�����ӽ���
index.html, Chart.bundle.js, jquery.min.js ��ѡ��master�����ҳ��,��Ҫ���ʱ�ҳ��ʱӦ����Щ�ļ�

��Master(���ΪServer)��Worker����ϵĲ��𷽷���ͬ

4. �����вο�
�ڲ���Ŀ¼��ִ������(������shift+�Ҽ��ڲ˵���ѡ��"�ڴ˴��������")
����ѡ���ʽ: --key=value�� -key value
(-�ĸ�����Ӱ������,key�����ִ�Сд)

4.1 ����Master��Worker���:
a.exe [ѡ���б�]

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
Master����ip,������Worker�����ָ��Master���.
��ѡ���Master�����Ч.
Ĭ��ֵΪ���ص�һ�����õ�ip.
Master����Worker��ͬһ�����ʱ,���Բ���ָ����ѡ��.

--p=port
--port=port
�����Ķ˿�.
serverĬ��ֵΪ3310.
workerĬ��ֵΪ3311.

--id=id
ʵ��id,ֻ��Worker�����Ч.
���ڱ�ʶͬһ������ϲ���Ĳ�ͬinstance.
δʹ��port����ʱ,Ĭ�϶˿�Ϊ Ĭ��ֵ+id.
Ĭ��ֵ0.

--c=cacheFilePath
--cache=cacheFilePath
�����ļ�·��.
ʹ��Ĭ�ϻ���Reader��Writerʱ��Ӧ��·��.
ע��:���Worker��ͬһ�������ʱ���д��ͬ�ļ�����������.
����ͨ����Ĭ��·����׷��id��ʹ��Worker��д��ͬCache.����
һ�㲻��Worker��ʹ�û���.
Ĭ��ֵΪ:dpeCache.txt.

--reset_cache=value
���½�Ĭ�ϻ���Writerʱ�Ƿ���������ļ�.
value=true��1��Ϊֵ��true,����ֵ����Ϊ��false.
Ĭ��ֵΪ:false

--http_port=port
--hp=port
����master���Ľ����ҳ��˿�.
Ĭ��ֵΪ:80

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

4.2 ͨ�����̼���������Worker��㲢���������״̬
pm.exe Ŀ�����·�� [ѡ���б�] -- �ӽ��̲����б�

--l=logLevel
--log=logLevel
��־����,һ��ֵȡ0��1.
Ĭ�ϼ���1.

--id=id
--id=minId-maxId
�ӽ��̵�id,id�ĺ����a.exe��ѡ���б�.
--id=id������minId=id,maxId=id
ͨ����ѡ����ͬһ���������������ӽ���.
Ĭ��ֵminId=0,maxId=0.


���� Ŀ�����·�� �� ѡ���б� ��˳���������.
��-- �ӽ��̲����б�����������.

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