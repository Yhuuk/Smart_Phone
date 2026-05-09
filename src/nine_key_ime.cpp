#include "nine_key_ime.h"
#include <ctype.h>  // 提供 tolower() 等字符处理函数
#include <Preferences.h>  //NVS管理的库

// -------------------- 词库条目结构 --------------------
// 每个条目表示：
// 1. 一个拼音 py
// 2. 这个拼音对应的若干候选字/词 cand[]
// 3. 实际候选个数 count
//
// 例如：
// {"ni", {"你", "呢", "泥", "拟", "妮"}, 5}
// 表示拼音 ni 有 5 个候选
//旧结构:一个拼音，8个指针。
//新结构:一个拼音，一整串候选字
//好处:节省flash，不再受8个候选限制
struct PinyinEntry {
    const char* py;         // 拼音字符串，如 "ni" / "hao" / "zhongguo",拼音指针
    const char* cands;    // 该拼音对应的候选字/词，最多放 8 个
};

struct  PhraseEntry{
    const char* py;     //连打拼音
    const char* cands;  //词组候选，使用 | 分隔
};

struct  ContextEntry{
    const   char* prev;     //上一个已经确认的字和词
    const   char* next;     //当前候选，如果命中就加分
    int16_t bonus;          //上下文加分
};



static const PinyinEntry KMainTable[] = {
    {"a", "啊腌阿"},
    {"ai", "哀哎唉埃挨爱癌矮碍艾蔼隘"},
    {"an", "俺安岸庵按暗案氨鞍"},
    {"ang", "昂肮"},
    {"ao", "傲凹奥懊拗澳熬袄"},
    {"ba", "八叭吧坝巴扒把拔捌爸疤笆罢耙芭跋霸靶"},
    {"bai", "伯拜掰摆柏白百败"},
    {"ban", "伴办半扮扳拌搬斑板版班瓣绊般颁"},
    {"bang", "傍帮梆棒榜磅绑膀蚌谤邦"},
    {"bao", "保刨剥包堡宝报抱暴瀑炮爆胞苞薄褒豹雹饱"},
    {"bei", "倍北卑备悲惫杯焙狈碑背臂被贝辈"},
    {"ben", "奔本笨"},
    {"beng", "崩泵绷蹦"},
    {"bi", "匕壁币庇弊彼必比毕毙泌璧痹碧秕秘笔荸蓖蔽辟逼避鄙闭鼻"},
    {"bian", "便匾变扁编蝙贬辨辩辫边遍鞭"},
    {"biao", "彪标膘表"},
    {"bie", "别憋瘪鳖"},
    {"bin", "宾彬滨濒缤鬓"},
    {"bing", "丙兵冰屏并柄病禀秉饼"},
    {"bo", "勃博卜拨搏播泊波渤玻簸脖膊舶菠跛驳"},
    {"bu", "不哺埠布怖捕步簿补部"},
    {"ca", "擦"},
    {"cai", "彩才材猜睬菜裁财踩采"},
    {"can", "参惨惭残灿蚕餐"},
    {"cang", "仓沧舱苍藏"},
    {"cao", "操曹槽糙草"},
    {"ce", "侧册厕测策"},
    {"ceng", "层曾蹭"},
    {"cha", "刹叉喳察岔差插杈查碴茬茶衩"},
    {"chai", "拆柴豺"},
    {"chan", "产单掺搀缠蝉铲阐颤馋"},
    {"chang", "倡偿厂唱场尝常敞昌猖畅肠长"},
    {"chao", "吵嘲巢抄朝潮炒绰超钞"},
    {"che", "尺彻扯撤澈车"},
    {"chen", "尘忱晨沉称臣衬趁辰陈"},
    {"cheng", "乘呈城惩成承撑橙澄盛秤程诚逞铛"},
    {"chi", "侈匙吃嗤弛持斥池痴翅耻赤迟驰齿"},
    {"chong", "充冲宠崇涌种虫重"},
    {"chou", "丑仇愁抽畴稠筹绸臭酬"},
    {"chu", "储出初厨处楚橱畜矗础触锄除雏"},
    {"chuai", "揣"},
    {"chuan", "串传喘川穿船"},
    {"chuang", "创幢床疮窗闯"},
    {"chui", "吹垂捶炊锤"},
    {"chun", "唇春椿淳纯蠢醇"},
    {"chuo", "戳"},
    {"ci", "伺刺慈次此瓷磁祠词赐辞雌"},
    {"cong", "丛从匆囱聪葱"},
    {"cou", "凑"},
    {"cu", "促卒簇粗醋"},
    {"cuan", "攒窜篡"},
    {"cui", "催崔悴摧粹翠脆"},
    {"cun", "存寸村"},
    {"cuo", "挫措搓撮锉错"},
    {"da", "大打搭瘩答达"},
    {"dai", "代呆带待怠戴歹袋逮"},
    {"dan", "丹但弹担掸旦氮淡石耽胆蛋诞"},
    {"dang", "党当挡档荡裆"},
    {"dao", "倒刀到叨导岛悼捣盗祷稻蹈道"},
    {"de", "地得德的"},
    {"deng", "凳灯登瞪等蹬邓"},
    {"di", "低嘀堤嫡帝底弟抵提敌涤滴笛第缔蒂递"},
    {"dian", "佃典垫奠店惦掂殿淀点玷电甸碘颠"},
    {"diao", "刁叼吊掉碉调钓雕"},
    {"die", "叠爹碟蝶谍跌"},
    {"ding", "丁叮定盯订钉锭顶鼎"},
    {"diu", "丢"},
    {"dong", "东冬冻动懂栋洞董"},
    {"dou", "兜抖斗痘蚪读豆逗都陡"},
    {"du", "堵妒度杜毒渡牍独督睹肚赌镀"},
    {"duan", "断段短端缎锻"},
    {"dui", "兑堆对队"},
    {"dun", "吨囤墩敦盹盾蹲钝顿"},
    {"duo", "哆垛堕多夺惰朵舵跺踱躲驮"},
    {"e", "俄噩恶愕扼蛾讹遏额饿鳄鹅"},
    {"en", "恩"},
    {"er", "二儿尔而耳贰饵"},
    {"fa", "乏伐发法筏罚阀"},
    {"fan", "凡反帆樊泛烦犯番矾繁翻范贩返饭"},
    {"fang", "仿坊妨房放方纺肪芳访防"},
    {"fei", "匪吠啡废沸肥肺菲诽费非飞"},
    {"fen", "份分吩坟奋忿愤氛焚粉粪纷芬"},
    {"feng", "丰冯凤奉封峰枫疯缝蜂讽逢锋风"},
    {"fo", "佛"},
    {"fou", "否"},
    {"fu", "付伏俘俯傅凫副咐复夫妇孵富幅府扶抚拂敷斧服浮父甫福符缚肤脯腐腹芙蝠袱覆负赋赴辅辐附麸"},
    {"ga", "咖夹"},
    {"gai", "丐改概溉盖芥该钙"},
    {"gan", "干感敢杆柑橄甘秆竿肝赶"},
    {"gang", "冈刚岗扛杠港纲缸肛钢"},
    {"gao", "告搞稿篙糕羔膏镐高"},
    {"ge", "个割各合哥戈搁格歌疙胳葛蛤阁隔革鸽"},
    {"gei", "给"},
    {"gen", "根跟"},
    {"geng", "埂更梗羹耕耿颈"},
    {"gong", "供公共功宫工巩弓恭拱攻汞蚣贡躬"},
    {"gou", "勾句垢够构沟狗苟购钩"},
    {"gu", "估古咕固姑孤故沽箍股菇谷贾辜雇顾骨鼓"},
    {"gua", "刮卦寡挂瓜褂"},
    {"guai", "乖怪拐"},
    {"guan", "关冠官惯棺灌管罐观贯馆"},
    {"guang", "光广逛"},
    {"gui", "刽归柜桂瑰硅规诡贵跪轨闺鬼龟"},
    {"gun", "棍滚"},
    {"guo", "国果涡裹过郭锅"},
    {"ha", "哈"},
    {"hai", "亥咳孩害海还骇"},
    {"han", "函含喊寒悍憨憾捍撼旱汉汗涵焊罕翰酣韩"},
    {"hang", "吭夯巷杭航行"},
    {"hao", "号嚎壕好毫浩耗蒿豪"},
    {"he", "何吓呵和喝核河盒禾荷褐贺赫鹤"},
    {"hei", "嘿黑"},
    {"hen", "很恨狠痕"},
    {"heng", "哼恒横衡"},
    {"hong", "哄宏洪烘红虹轰鸿"},
    {"hou", "侯候厚后吼喉猴"},
    {"hu", "乎互呼唬壶弧忽户护沪湖狐糊胡葫虎蝴"},
    {"hua", "划化华哗桦滑猾画花话"},
    {"huai", "坏徊怀槐淮"},
    {"huan", "唤宦幻患换欢涣焕环痪缓"},
    {"huang", "凰幌恍惶慌晃煌皇磺荒蝗谎黄"},
    {"hui", "会回徽恢悔惠慧挥晦毁汇溃灰秽绘茴蛔讳诲贿辉"},
    {"hun", "婚昏浑混荤魂"},
    {"huo", "伙惑或活火祸获豁货霍"},
    {"ji", "冀几击剂即及叽吉唧圾基奇妓嫉季寂寄己忌急技挤既期机极棘济激畸疾祭积稽箕籍系级纪继绩肌脊荠计讥记辑迹际集饥鲫鸡"},
    {"jia", "价佳假加嘉嫁家架枷甲稼茄荚钾颊驾"},
    {"jian", "件俭健兼减剑剪坚奸尖建拣捡柬检歼涧渐溅煎监碱简箭肩舰艰茧荐见贱践鉴键间"},
    {"jiang", "僵匠奖姜将强桨江浆疆缰蒋讲酱降"},
    {"jiao", "交侥剿叫嚼娇搅教校椒浇焦狡矫礁窖绞缴胶脚蕉觉角轿较郊酵饺骄"},
    {"jie", "介借劫姐届戒截捷接揭杰楷洁界皆秸竭结节街解诫阶"},
    {"jin", "仅今劲尽巾斤晋津浸禁筋紧襟谨近进金锦"},
    {"jing", "井京兢净境径惊敬景晶睛竞竟精经茎荆警镜阱靖静鲸"},
    {"jiong", "窘"},
    {"jiu", "久九就揪救旧灸玖疚究纠臼舅酒韭鸠"},
    {"ju", "举俱具剧局居巨惧拒拘据橘沮炬矩聚菊距锯鞠驹"},
    {"juan", "倦卷圈捐眷绢鹃"},
    {"jue", "倔决掘爵绝诀"},
    {"jun", "俊军君均峻竣菌钧骏"},
    {"ka", "卡"},
    {"kai", "凯开慨揩"},
    {"kan", "刊勘坎堪看砍"},
    {"kang", "康慷抗炕糠"},
    {"kao", "拷烤考铐靠"},
    {"ke", "克刻可坷壳客棵渴磕科苛蝌课颗"},
    {"ken", "啃垦恳肯裉"},
    {"keng", "坑"},
    {"kong", "孔恐控空"},
    {"kou", "口寇扣抠"},
    {"ku", "哭库枯窟苦裤酷"},
    {"kua", "垮夸挎胯跨"},
    {"kuai", "块快筷"},
    {"kuan", "宽款"},
    {"kuang", "况旷框狂眶矿筐"},
    {"kui", "亏傀愧盔窥葵魁"},
    {"kun", "困坤捆昆"},
    {"kuo", "廓扩括阔"},
    {"la", "啦喇垃拉腊落蜡辣"},
    {"lai", "来癞莱赖"},
    {"lan", "兰懒拦揽栏榄滥澜烂篮缆蓝览"},
    {"lang", "廊朗榔浪狼琅郎"},
    {"lao", "劳唠姥捞涝烙牢络老酪"},
    {"le", "乐了勒"},
    {"lei", "儡垒擂泪类累肋蕾雷"},
    {"leng", "冷棱楞"},
    {"li", "丽例俐利力励历厉厘吏哩李栗梨沥漓犁狸理璃痢砾礼离立篱粒荔莉里隶雳鲤黎"},
    {"lia", "俩"},
    {"lian", "帘廉怜恋敛炼练联脸莲连链镰"},
    {"liang", "两亮凉晾梁粮粱良谅辆量"},
    {"liao", "僚嘹寥撩料潦燎疗瞭缭聊辽镣"},
    {"lie", "列劣咧烈猎裂"},
    {"lin", "临凛吝林檩淋琳磷赁躏邻鳞"},
    {"ling", "令伶凌另岭灵玲翎菱蛉铃陵零领龄"},
    {"liu", "六刘柳榴流溜琉留瘤硫碌陆馏"},
    {"long", "咙垄弄拢窿笼聋胧隆龙"},
    {"lou", "娄搂楼漏篓陋露"},
    {"lu", "卢卤庐录炉绿芦虏赂路颅鲁鹿"},
    {"luan", "乱卵峦"},
    {"lun", "仑伦抡沦论轮"},
    {"luo", "啰洛箩罗萝螺裸逻锣骆骡"},
    {"lv", "侣吕屡履律旅氯滤率缕虑铝驴"},
    {"lve", "掠略"},
    {"ma", "吗妈抹摩玛码蚂蟆马骂麻"},
    {"mai", "买卖埋脉迈麦"},
    {"man", "幔慢曼满漫瞒蔓蛮馒"},
    {"mang", "忙氓盲芒茫莽"},
    {"mao", "冒帽毛猫矛茂茅貌贸铆锚"},
    {"me", "么"},
    {"mei", "妹媒媚昧枚梅楣每没煤玫眉糜美霉"},
    {"men", "们门闷"},
    {"meng", "孟朦梦檬猛盟萌蒙锰"},
    {"mi", "咪密弥眯米蜜觅谜迷靡"},
    {"mian", "免冕勉娩棉眠绵缅面"},
    {"miao", "妙庙描渺瞄秒苗藐"},
    {"mie", "灭蔑"},
    {"min", "悯敏民皿闽"},
    {"ming", "名命明螟铭鸣"},
    {"miu", "谬"},
    {"mo", "万墨寞摸摹无末模沫漠磨膜茉莫蘑陌馍魔默"},
    {"mou", "某谋"},
    {"mu", "亩募墓姆幕慕拇暮木母沐牡牧目睦穆"},
    {"na", "南呐哪娜拿捺纳那钠"},
    {"nai", "乃奈奶耐"},
    {"nan", "男难"},
    {"nang", "囊"},
    {"nao", "恼挠脑闹"},
    {"ne", "呢"},
    {"nei", "内馁"},
    {"nen", "嫩"},
    {"neng", "能"},
    {"ni", "你匿尼拟昵泥溺腻逆"},
    {"nian", "年念捻撵碾粘蔫"},
    {"niang", "娘酿"},
    {"niao", "尿鸟"},
    {"nie", "孽捏聂镊"},
    {"nin", "您"},
    {"ning", "凝宁拧柠泞狞"},
    {"niu", "扭牛纽钮"},
    {"nong", "农浓脓"},
    {"nu", "努奴怒"},
    {"nuan", "暖"},
    {"nuo", "懦挪糯诺"},
    {"nv", "女"},
    {"nve", "疟虐"},
    {"ou", "偶区呕欧殴藕鸥"},
    {"pa", "帕怕爬趴"},
    {"pai", "徘拍排派湃牌迫"},
    {"pan", "判叛攀潘畔盘盼胖"},
    {"pang", "乓庞旁螃"},
    {"pao", "咆抛泡袍跑"},
    {"pei", "佩培沛胚赔配陪"},
    {"pen", "喷盆"},
    {"peng", "彭捧朋棚澎烹砰硼碰篷膨蓬鹏"},
    {"pi", "僻劈匹啤坯屁批披疲皮脾譬霹"},
    {"pian", "偏片篇翩骗"},
    {"piao", "朴漂瓢票飘"},
    {"pie", "撇"},
    {"pin", "品拼聘贫频"},
    {"ping", "乒凭坪平瓶苹萍评"},
    {"po", "坡婆泼破颇魄"},
    {"pou", "剖"},
    {"pu", "仆圃扑普浦菩葡蒲谱铺"},
    {"qi", "七乞企其凄启嘁器契妻岂崎弃戚旗柒栖棋欺歧气汽泣漆畦砌祈脐起迄骑鳍齐"},
    {"qia", "恰掐洽"},
    {"qian", "前千嵌欠歉浅潜牵签纤谦谴迁遣钱钳铅黔"},
    {"qiang", "呛墙抢枪腔"},
    {"qiao", "乔侨俏峭巧悄憔撬敲桥瞧窍翘荞跷锹雀"},
    {"qie", "且切怯窃"},
    {"qin", "亲侵勤寝擒琴禽秦芹钦"},
    {"qing", "倾卿庆情擎晴氢清蜻请轻青顷"},
    {"qiong", "琼穷"},
    {"qiu", "丘囚求球秋蚯"},
    {"qu", "去取娶屈岖曲渠蛆趋趣躯驱"},
    {"quan", "全券劝拳权泉犬痊"},
    {"que", "却瘸确缺鹊"},
    {"qun", "群裙"},
    {"ran", "染然燃"},
    {"rang", "嚷壤攘瓤让"},
    {"rao", "扰绕饶"},
    {"re", "惹热"},
    {"ren", "人仁任刃忍纫认韧"},
    {"reng", "仍扔"},
    {"ri", "日"},
    {"rong", "冗容榕溶熔绒茸荣蓉融"},
    {"rou", "揉柔肉蹂"},
    {"ru", "乳儒入如蠕褥辱"},
    {"ruan", "软"},
    {"rui", "瑞蕊锐"},
    {"run", "润闰"},
    {"ruo", "弱若"},
    {"sa", "撒洒萨飒"},
    {"sai", "塞腮赛"},
    {"san", "三伞散"},
    {"sang", "丧嗓桑"},
    {"sao", "嫂扫搔臊骚"},
    {"se", "涩瑟色"},
    {"sen", "森"},
    {"seng", "僧"},
    {"sha", "傻厦啥杀杉沙煞砂纱霎"},
    {"shai", "晒筛"},
    {"shan", "删善山扇擅栅珊膳苫衫赡闪陕"},
    {"shang", "上伤商尚晌裳赏"},
    {"shao", "勺召哨少捎梢烧稍绍芍"},
    {"she", "奢射折拾摄涉社舌舍蛇设赊赦"},
    {"shei", "谁"},
    {"shen", "什伸呻婶审慎沈深渗甚申神绅肾身"},
    {"sheng", "剩升圣声牲生甥省笙绳胜"},
    {"shi", "世事似使侍势十史嗜士失始实室尸屎市师式恃拭施时是柿殖氏湿狮矢示虱蚀视誓识试诗适逝释食饰驶"},
    {"shou", "兽受售守寿手授收熟瘦首"},
    {"shu", "书叔墅属庶恕抒数暑曙术束枢树梳殊淑漱疏秫竖署舒蔬薯蜀赎输述黍鼠"},
    {"shua", "刷耍"},
    {"shuai", "帅摔甩蟀衰"},
    {"shuan", "拴栓涮"},
    {"shuang", "双爽霜"},
    {"shui", "水睡税说"},
    {"shun", "吮瞬顺"},
    {"shuo", "烁硕"},
    {"si", "丝司嘶四寺思撕斯死私肆饲"},
    {"song", "宋松耸讼诵送颂"},
    {"sou", "嗽搜艘"},
    {"su", "俗塑宿溯粟素缩肃苏诉速酥"},
    {"suan", "算蒜酸"},
    {"sui", "岁碎祟穗虽遂随隧髓"},
    {"sun", "孙损笋"},
    {"suo", "唆嗦所梭琐索锁"},
    {"ta", "他塌塔她它拓踏蹋"},
    {"tai", "台太态抬汰泰胎苔"},
    {"tan", "叹坛坦探摊昙檀毯滩潭炭痰瘫碳袒谈谭贪"},
    {"tang", "倘唐堂塘搪棠汤淌烫糖膛趟躺"},
    {"tao", "套掏桃涛淘滔萄讨逃陶"},
    {"te", "特"},
    {"teng", "疼腾藤誊"},
    {"ti", "体剃剔啼屉惕替梯涕踢蹄题"},
    {"tian", "填天恬添甜田舔"},
    {"tiao", "挑条笤跳"},
    {"tie", "帖贴铁"},
    {"ting", "亭停厅听庭廷挺艇蜓"},
    {"tong", "同彤捅桐桶痛瞳童筒统通铜"},
    {"tou", "偷头投透"},
    {"tu", "兔凸吐图土屠徒涂秃突途"},
    {"tuan", "团"},
    {"tui", "推腿蜕退颓"},
    {"tun", "吞屯臀"},
    {"tuo", "唾妥托拖椭脱驼鸵"},
    {"wa", "娃挖洼瓦蛙袜"},
    {"wai", "外歪"},
    {"wan", "丸婉完宛弯惋挽晚湾玩碗腕豌顽"},
    {"wang", "亡妄往忘旺望枉汪王网"},
    {"wei", "为伟伪位偎卫危味唯喂围委威尉尾巍微慰未桅猬畏纬维胃苇萎蔚薇谓违魏"},
    {"wen", "吻文温瘟稳紊纹蚊问闻"},
    {"weng", "嗡瓮翁"},
    {"wo", "卧我握沃窝蜗"},
    {"wu", "乌五伍侮务勿午吴呜坞屋巫悟捂晤梧武污物舞芜蜈诬误雾鹉"},
    {"xi", "习吸喜夕媳嬉希席息悉惜戏昔晰析洗溪熄熙牺犀稀细膝蟋袭西铣锡隙曦"},
    {"xia", "下侠匣夏峡暇狭瞎虾辖霞"},
    {"xian", "仙先县咸嫌宪弦掀显涎献现线羡腺舷衔贤锨闲限险陷馅鲜"},
    {"xiang", "乡享像厢向响想橡湘相祥箱翔详象镶项香"},
    {"xiao", "削哮啸嚣孝宵小效晓消淆硝笑箫肖萧销"},
    {"xie", "些写协卸叶屑懈挟携斜械楔歇泄泻胁蝎蟹血谐谢邪鞋"},
    {"xin", "信心新欣芯薪衅辛锌"},
    {"xing", "兴刑型姓幸形性星杏猩腥邢醒"},
    {"xiong", "兄凶匈汹熊胸雄"},
    {"xiu", "休修嗅朽秀绣羞袖锈"},
    {"xu", "叙吁婿序徐恤旭絮绪续蓄虚许酗需须"},
    {"xuan", "喧宣悬旋漩炫玄癣轩选"},
    {"xue", "学穴薛雪靴"},
    {"xun", "勋寻巡循旬殉汛熏训讯询迅逊驯"},
    {"ya", "亚压呀哑崖押涯牙芽蚜衙讶轧雅鸦鸭"},
    {"yan", "严厌咽唁堰奄宴岩延掩檐殷沿淹演炎烟焰燕盐眼研砚艳蜒衍言谚阎雁颜验"},
    {"yang", "仰养央扬杨样殃氧洋漾痒秧羊阳鸯"},
    {"yao", "吆咬夭妖姚摇窑约耀肴腰舀药要谣遥邀钥"},
    {"ye", "业也冶夜掖椰液爷腋谒野页"},
    {"yi", "一义乙亦亿以仪伊依倚医壹夷奕姨宜屹已异役忆意抑揖易椅毅溢疑疫益移绎翼肄胰艺蚁衣议译谊逸遗邑"},
    {"yin", "印吟因姻引淫瘾茵蚓银阴隐音饮"},
    {"ying", "婴应影映樱盈硬缨英荧莹莺萤营蝇赢迎颖鹦鹰"},
    {"yo", "哟"},
    {"yong", "佣勇咏庸拥永泳用蛹踊"},
    {"you", "优佑又友右尤幼幽忧悠有油游犹由诱邮"},
    {"yu", "与予于余喻域娱宇寓屿御愈愉愚榆欲浴淤渔狱玉羽育舆芋裕誉语豫迂逾遇郁隅雨预鱼"},
    {"yuan", "元冤原员园圆怨愿援渊源猿缘袁辕远院鸳"},
    {"yue", "岳悦月粤越跃阅"},
    {"yun", "云允匀孕晕蕴运酝陨韵"},
    {"za", "扎杂砸"},
    {"zai", "仔再在宰栽灾载"},
    {"zan", "咱暂赞"},
    {"zang", "脏葬赃"},
    {"zao", "凿噪早枣澡灶燥皂糟藻蚤躁造遭"},
    {"ze", "则择泽责"},
    {"zei", "贼"},
    {"zen", "怎"},
    {"zeng", "增憎综赠"},
    {"zha", "乍榨渣炸眨诈铡闸"},
    {"zhai", "债宅寨摘斋窄"},
    {"zhan", "占展崭战斩栈毡沾盏瞻站绽蘸"},
    {"zhang", "丈仗帐张彰掌杖樟涨章胀账障"},
    {"zhao", "兆找招昭沼照爪着罩赵"},
    {"zhe", "哲浙者蔗辙这遮"},
    {"zhen", "侦振斟枕榛珍疹真诊贞针镇阵震"},
    {"zheng", "争征怔拯挣政整正狰症睁筝蒸证郑"},
    {"zhi", "之侄值制只吱址帜志执指挚掷支旨智枝植止汁治滞直知秩稚窒纸织置职肢脂至致芝蜘质趾"},
    {"zhong", "中仲众忠盅终肿衷钟"},
    {"zhou", "周咒宙州帚昼洲皱粥肘舟轴骤"},
    {"zhu", "主住助嘱拄朱柱株注烛煮猪珠祝竹筑著蛀蛛诸贮逐铸驻"},
    {"zhua", "抓"},
    {"zhuan", "专撰砖赚转"},
    {"zhuang", "壮妆庄撞桩状装"},
    {"zhui", "坠椎缀赘追锥"},
    {"zhun", "准谆"},
    {"zhuo", "卓啄拙捉桌浊灼琢茁酌"},
    {"zi", "咨姊姿子字滋滓籽紫自资"},
    {"zong", "宗总棕纵踪"},
    {"zou", "奏揍走"},
    {"zu", "族祖租组诅足阻"},
    {"zuan", "钻"},
    {"zui", "嘴最罪醉"},
    {"zun", "尊遵"},
    {"zuo", "作做坐左座昨"},
};

static const PinyinEntry KAliasTable[] = {
    {"beng", "蚌"},
    {"bi", "臂"},
    {"bo", "伯剥柏薄"},
    {"bu", "卜堡"},
    {"cen", "参"},
    {"chai", "差"},
    {"cheng", "称"},
    {"chi", "尺"},
    {"chuo", "绰"},
    {"ci", "差"},
    {"dai", "大"},
    {"dan", "单"},
    {"dang", "铛"},
    {"dei", "得"},
    {"deng", "澄"},
    {"di", "地的"},
    {"du", "读都"},
    {"duo", "度"},
    {"e", "阿"},
    {"fu", "佛"},
    {"ge", "盖"},
    {"ha", "蛤"},
    {"he", "合"},
    {"heng", "行"},
    {"hng", "哼"},
    {"hu", "和核"},
    {"huan", "还"},
    {"huo", "和"},
    {"ji", "给"},
    {"jia", "夹贾"},
    {"jiang", "虹"},
    {"jie", "价芥"},
    {"jing", "劲颈"},
    {"ju", "句柜车"},
    {"jue", "嚼觉角"},
    {"jun", "龟"},
    {"ka", "咖"},
    {"kai", "楷"},
    {"kang", "扛"},
    {"ke", "咳"},
    {"keng", "吭"},
    {"kuai", "会"},
    {"kui", "溃"},
    {"lao", "落"},
    {"lei", "勒"},
    {"liang", "俩"},
    {"liao", "了"},
    {"ling", "棱"},
    {"lu", "碌陆露"},
    {"luo", "烙络落"},
    {"lv", "绿"},
    {"man", "埋"},
    {"mi", "泌秘糜"},
    {"mo", "抹摩没脉"},
    {"mu", "模"},
    {"nan", "南"},
    {"ne", "哪"},
    {"ni", "呢"},
    {"niu", "拗"},
    {"nong", "弄"},
    {"nuo", "娜"},
    {"pa", "扒耙"},
    {"pan", "番"},
    {"pang", "磅胖膀"},
    {"pao", "刨炮"},
    {"pi", "否辟"},
    {"pian", "便扁"},
    {"ping", "屏"},
    {"po", "朴泊繁迫"},
    {"pu", "堡朴瀑脯"},
    {"qi", "奇期荠"},
    {"qia", "卡"},
    {"qiang", "强"},
    {"qiao", "壳"},
    {"qie", "茄"},
    {"qing", "亲"},
    {"qiu", "仇龟"},
    {"qu", "区"},
    {"quan", "圈"},
    {"que", "雀"},
    {"se", "塞"},
    {"sha", "刹"},
    {"shai", "色"},
    {"shan", "单杉"},
    {"shen", "参"},
    {"sheng", "盛"},
    {"shi", "什匙拾石"},
    {"shu", "熟"},
    {"shuai", "率"},
    {"shui", "谁"},
    {"shuo", "数说"},
    {"si", "伺似"},
    {"sui", "尿"},
    {"suo", "缩"},
    {"tan", "弹"},
    {"tao", "叨"},
    {"ti", "提"},
    {"tiao", "调"},
    {"tong", "洞"},
    {"tun", "囤"},
    {"tuo", "拓驮"},
    {"wan", "万蔓"},
    {"wo", "涡"},
    {"wu", "恶无"},
    {"xi", "系"},
    {"xia", "厦吓"},
    {"xian", "洗纤见"},
    {"xiang", "巷降"},
    {"xiao", "校"},
    {"xie", "解"},
    {"xing", "省行"},
    {"xiu", "宿臭"},
    {"xu", "畜"},
    {"xuan", "券"},
    {"xue", "削血"},
    {"yan", "腌铅"},
    {"yao", "侥疟"},
    {"ye", "叶咽"},
    {"yi", "尾艾"},
    {"yin", "殷"},
    {"yong", "涌"},
    {"yu", "吁尉蔚"},
    {"yue", "乐约钥"},
    {"yun", "员"},
    {"zan", "攒"},
    {"zang", "藏"},
    {"zeng", "曾"},
    {"zha", "喳扎查栅轧"},
    {"zhai", "择祭"},
    {"zhan", "粘"},
    {"zhang", "长"},
    {"zhao", "召朝"},
    {"zhe", "折着"},
    {"zhi", "殖氏识"},
    {"zhong", "种重"},
    {"zhu", "术"},
    {"zhua", "爪"},
    {"zhuan", "传"},
    {"zhuang", "幢"},
    {"zhuo", "着著"},
    {"zi", "仔吱"},
    {"zong", "综"},
    {"zu", "卒"},
    {"zuo", "撮琢"},
};


static const PhraseEntry KPhraseTable[] = {
    {"nihao", "你好|你号|拟好"},
    {"nihaoma", "你好吗"},
    {"nimen", "你们"},
    {"nimenhao", "你们好"},
    {"zaijian", "再见"},
    {"xiexie", "谢谢"},
    {"duibuqi", "对不起"},
    {"meiguanxi", "没关系"},
    {"women", "我们"},
    {"dajia", "大家"},
    {"zhongguo", "中国"},
    {"jintian", "今天"},
    {"mingtian", "明天"},
    {"zuotian", "昨天"},
    {"xianzai", "现在"},
    {"zheli", "这里"},
    {"nali", "那里"},
    {"zenmeyang", "怎么样"},
    {"weishenme", "为什么"},
    {"shenme", "什么"},
    {"keyi", "可以"},
    {"bukeyi", "不可以"},
    {"buxing", "不行"},
    {"zhidao", "知道"},
    {"buzhidao", "不知道"},
    {"henhao", "很好"},
    {"buhao", "不好"},
    {"haode", "好的"},
    {"meiwenti", "没问题"},
    {"meishi", "没事"},
    {"meishide", "没事的"},
    {"zhende", "真的"},
    {"dangran", "当然"},
    {"keneng", "可能"},
    {"yinggai", "应该"},
    {"yijing", "已经"},
    {"mashang", "马上"},
    {"yiqi", "一起"},
    {"dengdeng", "等等"},
    {"huijia", "回家"},
    {"shangban", "上班"},
    {"xiaban", "下班"},
    {"chifan", "吃饭"},
    {"shuijiao", "睡觉"},
    {"xuexi", "学习"},
    {"gongzuo", "工作"},
    {"xiuxi", "休息"},
    {"shuohua", "说话"},
    {"liaotian", "聊天"},
    {"dakai", "打开"},
    {"guanbi", "关闭"},
    {"kaishi", "开始"},
    {"jieshu", "结束"},
    {"shuru", "输入"},
    {"shanchu", "删除"},
    {"fasong", "发送"},
    {"jieshou", "接收"},
    {"lianjie", "连接"},
    {"duankai", "断开"},
    {"chenggong", "成功"},
    {"shibai", "失败"},
    {"cuowu", "错误"},
    {"wangluo", "网络"},
    {"xinhao", "信号"},
    {"dianyuan", "电源"},
    {"shezhi", "设置"},
    {"xitong", "系统"},
    {"caidan", "菜单"},
    {"fanhui", "返回"},
    {"queren", "确认"},
    {"quxiao", "取消"},
    {"xihuan", "喜欢"},
    {"buxihuan", "不喜欢"},
    {"gaoxing", "高兴"},
    {"nanguo", "难过"},
    {"shengqi", "生气"},
    {"zaima", "在吗"},
    {"youma", "有吗"},
    {"meiyou", "没有"},
    {"youyidian", "有一点"},
    {"chabuduo", "差不多"},
    {"chayidian", "差一点"},
    {"taihaole", "太好了"},
    {"tainanle", "太难了"},
    {"taimafanle", "太麻烦了"},
    {"taimanle", "太慢了"},
    {"taikuaile", "太快了"},
    {"buzhaoji", "不着急"},
    {"manmanlai", "慢慢来"},
    {"kuaiyidian", "快一点"},
    {"zaishiyici", "再试一次"},
    {"chongxinkaishi", "重新开始"},
    {"chongxinlianjie", "重新连接"},
    {"lianjiechenggong", "连接成功"},
    {"lianjieshibai", "连接失败"},
    {"fasongchenggong", "发送成功"},
    {"fasongshibai", "发送失败"},
    {"jieshouchenggong", "接收成功"},
    {"jiazaizhong", "加载中"},
    {"chushihua", "初始化"},
    {"weilianjie", "未连接"},
    {"yilianjie", "已连接"},
    {"qingchongxinshuru", "请重新输入"},
    {"shurucuowu", "输入错误"},
    {"mimacuowu", "密码错误"},
    {"wangluocuowu", "网络错误"},
    {"xitongcuowu", "系统错误"},
    {"baocunchenggong", "保存成功"},
    {"baocunshibai", "保存失败"},
    {"shezhichenggong", "设置成功"},
    {"shezhishibai", "设置失败"},
    {"huifumoren", "恢复默认"},
    {"fanhuishangyiji", "返回上一级"},
    {"xiayibu", "下一步"},
    {"shangyibu", "上一步"},
    {"jixu", "继续"},
    {"tingzhi", "停止"},
    {"zanting", "暂停"},
    {"wancheng", "完成"},
    {"zhengzaichuli", "正在处理"},
    {"chulizhong", "处理中"},
    {"qingshaohou", "请稍后"},
    {"banbengengxin", "版本更新"},
    {"jianchagengxin", "检查更新"},
    {"didianliang", "低电量"},
    {"chongdianzhong", "充电中"},
    {"dianliangbuzu", "电量不足"},
    {"xinhaobuhao", "信号不好"},
    {"wangluolianjie", "网络连接"},
    {"lanya", "蓝牙"},
    {"redian", "热点"},
    {"yonghuming", "用户名"},
    {"mima", "密码"},
    {"yanzhengma", "验证码"},
    {"liaotianjilu", "聊天记录"},
    {"fasongxiaoxi", "发送消息"},
    {"yuyinxiaoxi", "语音消息"},
    {"tupian", "图片"},
    {"shipin", "视频"},
    {"wenjian", "文件"},
    {"lianxiren", "联系人"},
    {"qunliao", "群聊"},
    {"gerenzhongxin", "个人中心"},
    {"shoucang", "收藏"},
    {"sousuo", "搜索"},
    {"xiazai", "下载"},
    {"shangchuan", "上传"},
    {"gengxin", "更新"},
    {"anzhuang", "安装"},
    {"xiezai", "卸载"},
    {"tiaoshi", "调试"},
    {"chuankou", "串口"},
    {"lianjieshebei", "连接设备"},
    {"dakailanya", "打开蓝牙"},
    {"guanbilanya", "关闭蓝牙"},
    {"dakairedian", "打开热点"},
    {"guanbiredian", "关闭热点"},
    {"dakaiwangluo", "打开网络"},
    {"guanbiwangluo", "关闭网络"},
    {"wozai", "我在"},
    {"nizai", "你在"},
    {"tazai", "他在"},
    {"womenzai", "我们在"},
    {"wolaile", "我来了"},
    {"wodaole", "我到了"},
    {"wozhidaole", "我知道了"},
    {"wobuzhidao", "我不知道"},
    {"womingbaile", "我明白了"},
    {"woxiangni", "我想你"},
    {"woxiangwenyixia", "我想问一下"},
    {"woxiangchifan", "我想吃饭"},
    {"woyaohuijia", "我要回家"},
    {"woyaochuqu", "我要出去"},
    {"woyaoshangban", "我要上班"},
    {"woyaoxiaban", "我要下班"},
    {"woyaoshuijiao", "我要睡觉"},
    {"woyaoxuexi", "我要学习"},
    {"wozaichifan", "我在吃饭|我再吃饭"},
    {"wozaixuexi", "我在学习"},
    {"wozaigongzuo", "我在工作"},
    {"wozailushang", "我在路上"},
    {"wozaijiali", "我在家里"},
    {"womashangdao", "我马上到"},
    {"woxianzoule", "我先走了"},
    {"woxianchifanle", "我先吃饭了"},
    {"woxianshuile", "我先睡了"},
    {"woxianqumangle", "我先去忙了"},
    {"wohuijiale", "我回家了"},
    {"wohuilaile", "我回来了"},
    {"womashanghuilai", "我马上回来"},
    {"wochufale", "我出发了"},
    {"wodaojiale", "我到家了"},
    {"wodaogongsile", "我到公司了"},
    {"nizaiganma", "你在干嘛|你在干吗"},
    {"nizaizuoshenme", "你在做什么"},
    {"nichifanlema", "你吃饭了吗"},
    {"nishuijiaolema", "你睡觉了吗"},
    {"nishenmeshihoulai", "你什么时候来"},
    {"nishenmeshihouhuiqu", "你什么时候回去"},
    {"nixianmang", "你先忙"},
    {"nishuodedui", "你说得对"},
    {"nishuodeyoudaoli", "你说得有道理"},
    {"nizainali", "你在哪里"},
    {"nishishui", "你是谁"},
    {"nixianchifanba", "你先吃饭吧"},
    {"nixianxiuxiba", "你先休息吧"},
    {"womenyiqi", "我们一起"},
    {"womenyiqiqu", "我们一起去"},
    {"womenhuijia", "我们回家"},
    {"womenchifanba", "我们吃饭吧"},
    {"womenqukankan", "我们去看看"},
    {"womenlianxiyixia", "我们联系一下"},
    {"jintianzenmeyang", "今天怎么样"},
    {"jintiantianqi", "今天天气"},
    {"jintiantianqihenhao", "今天天气很好"},
    {"jintiantianqibucuo", "今天天气不错"},
    {"jintianyoukongma", "今天有空吗"},
    {"jintianwanshangyoukongma", "今天晚上有空吗"},
    {"mingtianjian", "明天见"},
    {"mingtianzaishuo", "明天再说"},
    {"mingtianzaoshang", "明天早上"},
    {"mingtianwanshang", "明天晚上"},
    {"zuotianwanshang", "昨天晚上"},
    {"xianzaikeyima", "现在可以吗"},
    {"xianzaibuxing", "现在不行"},
    {"xianzaikaishi", "现在开始"},
    {"dengyixia", "等一下"},
    {"dengwoyixia", "等我一下"},
    {"shaodengyixia", "稍等一下"},
    {"haodehaode", "好的好的"},
    {"keyikeyi", "可以可以"},
    {"butaihao", "不太好"},
    {"feichanghao", "非常好"},
    {"tebiehao", "特别好"},
    {"zhenbucuo", "真不错"},
    {"haojiubujian", "好久不见"},
    {"xinkule", "辛苦了"},
    {"mafannile", "麻烦你了"},
    {"baituole", "拜托了"},
    {"qingwenyixia", "请问一下"},
    {"qingshaodeng", "请稍等"},
    {"qingjin", "请进"},
    {"qingzuo", "请坐"},
    {"qingkanyixia", "请看一下"},
    {"qinggaosuwo", "请告诉我"},
    {"bangwoyixia", "帮我一下"},
    {"banggemang", "帮个忙"},
    {"fageiwo", "发给我"},
    {"faguolai", "发过来"},
    {"shoudaole", "收到了"},
    {"kandaole", "看到了"},
    {"tingdaole", "听到了"},
    {"zhaodaole", "找到了"},
    {"meikandao", "没看到"},
    {"meitingqing", "没听清"},
    {"zaishuoyibian", "再说一遍"},
    {"shenmeyisi", "什么意思"},
    {"zenmehuishi", "怎么回事"},
    {"zenmele", "怎么了"},
    {"weishenmezheyang", "为什么这样"},
    {"yuanlairuci", "原来如此"},
    {"bushizheyang", "不是这样"},
    {"jiushizheyang", "就是这样"},
    {"xianzheyangba", "先这样吧"},
    {"huitouzaishuo", "回头再说"},
    {"wandianzaishuo", "晚点再说"},
    {"lushangxiaoxin", "路上小心"},
    {"zhuyianquan", "注意安全"},
    {"yilushunfeng", "一路顺风"},
    {"daojialema", "到家了吗"},
    {"daogongsilema", "到公司了吗"},
};


static const ContextEntry KContextTable[] = {
    {"你", "好", 320},
    {"你", "们", 220},
    {"你", "在", 220},
    {"你在", "干嘛", 260},
    {"你在", "做什么", 240},
    {"我", "们", 220},
    {"我", "在", 260},
    {"我", "要", 220},
    {"我", "想", 200},
    {"我", "先", 180},
    {"我在", "吃饭", 300},
    {"我在", "学习", 260},
    {"我在", "工作", 260},
    {"吃", "饭", 260},
    {"睡", "觉", 260},
    {"学", "习", 240},
    {"工", "作", 240},
    {"上", "班", 260},
    {"下", "班", 260},
    {"回", "家", 260},
    {"请", "问一下", 260},
    {"请", "稍等", 240},
    {"请", "看一下", 220},
    {"请", "告诉我", 220},
    {"没", "关系", 240},
    {"没", "问题", 220},
    {"没", "事", 220},
    {"再", "见", 260},
    {"再", "说一遍", 220},
    {"明天", "见", 240},
    {"明天", "早上", 220},
    {"明天", "晚上", 220},
    {"今天", "天气", 260},
    {"今天天气", "很好", 280},
    {"网络", "连接", 240},
    {"重新", "连接", 220},
    {"连接", "成功", 260},
    {"连接", "失败", 240},
    {"发送", "成功", 240},
    {"发送", "失败", 220},
    {"返回", "上一级", 220},
    {"打", "开", 160},
    {"关", "闭", 160},
};


//if((firstByte & 0x80) == 0x00)如果写成 if((firstByte) & 0x80 == 0x00)代码拼音汉字乱码，这个优先级 
static uint8_t utf8CharBytes(uint8_t firstByte){
        if((firstByte & 0x80) == 0x00)     return 1;      //0xxxxxxx
        if((firstByte & 0xE0) == 0xC0)     return 2;      //110XXXXX
        if((firstByte & 0xF0) == 0xE0)     return 3;      //1110XXXX
        if((firstByte & 0xF8) == 0xF0)     return 4;      //11110XXX
        return 1;
}

// 词库总条目数
// sizeof(kTable)      = 整个数组占用的字节数
// sizeof(kTable[0])   = 一个条目占用的字节数
// 二者相除得到“条目个数”
//
// size_t 是 C/C++ 里专门表示“大小/长度”的无符号类型
// 在 ESP32 这种 32 位平台上，通常是 4 字节
//sizeof(kTable[0])和sizeof(kTable[1])这些都是一样的大小，因为它们都是结构体中的PinyinEntry的元素，所以底层大小是一样的
static const size_t KMainTableSize = sizeof(KMainTable) / sizeof(KMainTable[0]);
static const size_t KAliasTableSize = sizeof(KAliasTable) / sizeof(KAliasTable[0]);
static const size_t KPhraseTableSize = sizeof(KPhraseTable) / sizeof(KPhraseTable[0]);
static const size_t KContextTableSize = sizeof(KContextTable) / sizeof(KContextTable[0]);


static const char* kImelearnNamespace = "imelearn";
static const char* kImelearnBlobkey   = "blob";


static const char* englishLettersForKey(char key){
    switch (key)
    {
        case '2': return "abc";
        case '3': return "def";
        case '4': return "ghi";
        case '5': return "jkl";
        case '6': return "mno";
        case '7': return "pqrs";
        case '8': return "tuv";
        case '9': return "wxyz";

        default: return nullptr;  //nullptr是个什么值
        
    }
}

//空格符直接电脑键盘打出空格就是空格符号了
static const char* commonSymbolsForkey()
{
    return  "-/(),.!?:;\"' ~&%$#@'";   //"符号需要\"来
}

// 初始化输入法
void NineKeyIME::begin() {
    _mode = INPUT_CN;   // 默认进入中文模式
    _text = "";         // 已经确认好的正文清空(初始化文本框)
    _digits = "";       // 当前数字串清空
    _textCursor = 0;    //文本框光标初始
    _cnStage = CN_STAGE_PY;  //拼音候选的状态
    clearCandidates();  // 候选缓存清空
    _sendFlag = false;  // 发送请求标志清零
    _backFlag = false;  // 返回请求标志清零

    loadLearningFromNvs();  //在开机或复位时，从NVS把历史频次表恢复
}


String NineKeyIME::packLearningData() const{

    /********
     * 打包格式:
     * 第一行:版本号
     * 第二行:上一个已上屏词
     * 第三行:频次表条数
     * 后续:每行一个"词条\t频次"
     * 
     */

    String blob;
    blob.reserve(2048); //提前给 blob 这个 String 预留 2048 字节的内存空间

    blob += "V1\n";
    blob += _lastCommitted;
    blob += "\n";
    blob += String(_userFreqCount);
    blob += "\n";

    for(uint8_t i = 0; i < _userFreqCount; i++){

            //continue是循环里的关键词，作用:跳过本次循环后面的剩余代码，直接进入下一次循环
            if(_userFreq[i].text.length() == 0) continue;
            if(_userFreq[i].freq == 0) continue;

            blob += _userFreq[i].text;
            blob += "\t";                       //这个是制表符
            blob += String(_userFreq[i].freq);
            blob += "\n";
    }

    return blob;
}


void NineKeyIME::unpackLearningData(const String& blob) {
    _lastCommitted = "";
    _userFreqCount = 0;
    for (uint8_t i = 0; i < MAX_USER_FREQ; i++) {
        _userFreq[i].text = "";
        _userFreq[i].freq = 0;
    }

    if (blob.length() == 0) return;

    int pos = 0;

    auto readLine = [&](String &out) {
        int nl = blob.indexOf('\n', pos);
        if (nl < 0) {
            out = blob.substring(pos);
            pos = blob.length();
        } else {
            out = blob.substring(pos, nl);
            pos = nl + 1;
        }
    };

    String line;
    readLine(line);
    if (line != "V1") {
        // 版本不匹配，直接忽略，避免旧格式把数据读坏
        return;
    }

    readLine(_lastCommitted);

    readLine(line);
    int declaredCount = line.toInt();
    if (declaredCount < 0) declaredCount = 0;

    while (pos < blob.length() && _userFreqCount < MAX_USER_FREQ) {
        readLine(line);
        if (line.length() == 0) continue;

        int tabPos = line.lastIndexOf('\t');
        if (tabPos <= 0) continue;

        String text = line.substring(0, tabPos);
        uint16_t freq = static_cast<uint16_t>(line.substring(tabPos + 1).toInt());
        if (text.length() == 0) continue;
        if (freq == 0) freq = 1;

        _userFreq[_userFreqCount].text = text;
        _userFreq[_userFreqCount].freq = freq;
        _userFreqCount++;
    }

    // declaredCount 只是辅助信息，不强依赖它，真正以解析出来的条目为准。
    (void)declaredCount;
}

//从"imelearn/blob"读字符串，再解包
void NineKeyIME::loadLearningFromNvs(){
        Preferences prefs;
        if(!prefs.begin(kImelearnNamespace,true)){
            return;
        }

        String  blob = prefs.getString(kImelearnBlobkey,"");
        prefs.end();

        unpackLearningData(blob);
}

void NineKeyIME::saveLearningToNvs() const{
        Preferences prefs;
        if(!prefs.begin(kImelearnNamespace,false)){
                return;
        }

        String  blob = packLearningData();
        prefs.putString(kImelearnBlobkey,blob);
        prefs.end();
}

// 清空已经确认上屏的正文
//文本区光标复位
//文本清空后，上一词上下文也应该一起被清除，否则下一次重启还保留上下文，会导致联想方向错误
void NineKeyIME::clearText() {
    _text = "";
    _textCursor = 0;
    _lastCommitted = "";
    saveLearningToNvs();
}

// 清空当前这一轮还未确认的输入内容
// 即：清空数字串 + 候选
// 但不影响已经确认好的正文 _text
void NineKeyIME::clearComposition() {
    _digits = "";                       //_digits存放的是候选的数字拼音
    _cnStage = CN_STAGE_PY;             ///
    clearCandidates();
}

// 取走发送标志并自动清零
// 这是一个“一次性事件消费”的设计，避免 loop() 里重复触发发送
bool NineKeyIME::takeSendFlag() {
    bool v = _sendFlag;
    _sendFlag = false;
    return v;
}

// 取走返回标志并自动清零
bool NineKeyIME::takeBackFlag() {
    bool v = _backFlag;
    _backFlag = false;
    return v;
}

// 返回当前模式的中文名称，供界面显示
String NineKeyIME::modeName() const {
    switch (_mode) {
        case INPUT_CN:  return "中文";
        case INPUT_EN:  return "英文";
        case INPUT_NUM: return "数字";
        default:        return "未知";
    }
}

// 切换到下一个模式
// 顺序：中文 -> 英文 -> 数字 -> 中文 ...
//static_cast<目标类型>(要转换的值)。是显式类型转换
void NineKeyIME::nextMode() {
    _mode = static_cast<InputMode>((_mode + 1) % 3);  //static_cast<InputMode>是强行把后面()里的值转换回InputMode枚举的类型。

    // 切模式时清掉当前拼音串与候选，避免跨模式残留状态造成混乱
    clearComposition();
}

// 按键统一入口
// 所有按键都从这里进入，再根据当前模式分流到不同处理函数
void NineKeyIME::onKey(char key) {
    switch (_mode) {
        case INPUT_CN:  handleChineseKey(key); break;
        case INPUT_EN:  handleEnglishKey(key); break;
        case INPUT_NUM: handleNumberKey(key);  break;
    }
}


void NineKeyIME::handleChineseKey(char key) {
    // 2~9：输入数字，重建“拼音候选 + 当前拼音对应的汉字候选”
    if (key >= '2' && key <= '9') {
        _digits += key;
        rebuildCandidates();
        _cnStage = CN_STAGE_PY;   // 新输入后，默认回到拼音行
        return;
    }

    // 1：符号，直接当作“汉字候选行”来处理
    if (key == '1') {
        clearComposition();
        _digits = "1";
        _cnStage = CN_STAGE_HZ;

        const char* symbols = commonSymbolsForkey();
        for (uint8_t i = 0; symbols[i] != '\0'; i++) {
            char one[2] = {symbols[i], '\0'};
            addCandidate("sym", one);
        }

        _hzSel = 0;
        return;
    }

    switch (key) {
        case 'A':
            if (_digits.length() > 0) {
                if (_cnStage == CN_STAGE_PY) {
                    if (_pyCount > 0) {
                        _pySel = (_pySel == 0) ? (_pyCount - 1) : (_pySel - 1);
                        rebuildHanziFromSelectedPinyin();
                    }
                } else {
                    if (_hzCount > 0) {
                        _hzSel = (_hzSel == 0) ? (_hzCount - 1) : (_hzSel - 1);
                    }
                }
            } else {
                moveTextCursorLeft();
            }
            break;

        case 'B':
            if (_digits.length() > 0) {
                if (_cnStage == CN_STAGE_PY) {
                    if (_pyCount > 0) {
                        _pySel = (_pySel + 1) % _pyCount;
                        rebuildHanziFromSelectedPinyin();
                    }
                } else {
                    if (_hzCount > 0) {
                        _hzSel = (_hzSel + 1) % _hzCount;
                    }
                }
            } else {
                moveTextCursorRight();
            }
            break;

        case 'C':
            nextMode();
            break;

        case 'D':
            if (_digits.length() == 0) break;

            // 第一次 D：从拼音行切到汉字行
            if (_cnStage == CN_STAGE_PY) {
                if (_pyCount > 0) {
                    _cnStage = CN_STAGE_HZ;
                    if (_hzCount == 0) {
                        rebuildHanziFromSelectedPinyin();
                    }
                }
            }
            // 第二次 D：确认汉字
            else {
                commitCurrentCandidate();
            }
            break;

        case '*':
            if (_digits.length() > 0) {
                // 如果当前在汉字行，先退回拼音行
                if (_cnStage == CN_STAGE_HZ && _pyCount > 0) {
                    _cnStage = CN_STAGE_PY;
                } else {
                    _digits.remove(_digits.length() - 1);
                    rebuildCandidates();
                }
            } else {
                deleteBeforeTextCursor();
            }
            break;

        case '0':
            if (_digits.length() > 0) {
                clearComposition();
            } else {
                _backFlag = true;
            }
            break;

        case '#':
            _sendFlag = true;
            break;

        default:
            break;
    }
}


// -------------------- 英文模式（当前只是占位） --------------------
// 这里还没有实现真正的英文九键/多击英文输入，先保留模式结构
void NineKeyIME::handleEnglishKey(char key) {

        // 1：输入各类符号
    if (key == '1') {
        clearComposition();
        _digits = "1";          //_digits是一个

        const char* symbols = commonSymbolsForkey();

        for(uint8_t i = 0; symbols[i] != '\0'; i++){
                    char one[2] = {symbols[i],'\0'};
                    addCandidate("sym",one);  //sym是放进拼音数组，但是符号有单独的数组，所以不需要再把拼音转换成数字进行匹配
        }

        _hzSel = 0;
        return;
    }
    //输入相应按键返回对应的英文字母
    if (key >= '1' && key <= '9') {
        const char* letters = englishLettersForKey(key);

        //清理上一轮英文候选，避免残留
        clearComposition();

        _digits = String(key);    //把key转换成Arduino中的字符串。

        if(letters){

            for(uint8_t i = 0; letters[i] != '\0'; i++){
                char one[2] = { letters[i], '\0'};
                addCandidate(letters,one);       //这一部它使用了汉字生成的函数，可以自己再重新写一个
            }

            _hzSel = 0;
        }
        return;
    }

        switch(key){

            //按键A左移
            case 'A':
                    if(_hzCount > 0){
                     _hzSel = (_hzSel ==0) ? (_hzCount -1) : (_hzSel -1);
                    }else{
                        moveTextCursorLeft();
                    }
                break;

            //按键B右移
            case 'B':
                    if(_hzCount > 0){
                        _hzSel = (_hzSel == (_hzCount -1)) ? (_hzSel = 0) : (_hzSel +1);
                        //_sel = (_sel +1) % _candCount; //右移
                    }else{
                        moveTextCursorRight();
                    }
                 break;

                //切换模式直接调用nextMode()内部进行切换
              case 'C':
                    nextMode();
                break;

                //把当前候选英文字符插入至光标后
              case 'D':
                    commitCurrentCandidate();
                break;


              case '*':
                    if(_hzCount > 0){
                            clearComposition();  //一次性清空英文候选（就是取消这次英文输入）
                    }else{
                        deleteBeforeTextCursor();  //删除光标前的字符
                    }
                break;

              case '0':
                    if(_hzCount > 0){
                            clearComposition();
                    }else{
                        _backFlag  = true;      //这个标志位应该还没用上，应该是之后用来界面返回的
                    }
                break;

              case '#':
                    _sendFlag = true;
                break;

                default:
                break;
        }
}

// -------------------- 数字模式 --------------------
// 数字模式最简单：数字键直接作为正文输入
//数字模式下0~9都是作为数字正常输入
void NineKeyIME::handleNumberKey(char key) {
    if (key >= '0' && key <= '9') {
        // _text += key;
        insertAtTextCursor((String)key);
        return;
    }

    switch (key) {

        //按键A左移
        case 'A':
             moveTextCursorLeft();
        break;

        //按键B右移
        case 'B':     
            moveTextCursorRight();
        break;
                
        case 'C':
            nextMode();
            break;

        case 'D':
            commitCurrentCandidate();
        break;

        case '*':
            deleteBeforeTextCursor();
            break;

        // case '0':
        //     _backFlag = true;
        //     break;

        case '#':
            _sendFlag = true;
            break;

        default:
            break;
    }
}

// -------------------- 候选缓存管理 --------------------

// // 清空当前候选缓存 

void NineKeyIME::clearCandidates() {
    _pyCount = 0;
    _pySel   = 0;

    _hzCount = 0;
    _hzSel   = 0;

    for (uint8_t i = 0; i < MAX_PY_CAND; i++) {
        _candPy[i] = "";
        _pyScore[i] = 0;
    }

        for (uint8_t i = 0; i < MAX_HZ_CAND; i++) {
        _candText[i] = "";
        _hzScore[i] = 0;
    }

}

/************************新增***************************/
void NineKeyIME::clearHanziCandidates() {
    _hzCount = 0;
    _hzSel = 0;

    for (uint8_t i = 0; i < MAX_HZ_CAND; i++) {
        _candText[i] = "";
        _hzScore[i] = 0;
    }
}


bool NineKeyIME::pyCandidateExists(const char* py) const {
    for (uint8_t i = 0; i < _pyCount; i++) {
        if (_candPy[i] == py) return true;
    }
    return false;
}



void NineKeyIME::addPyCandidate(const char* py, int16_t score) {
    int8_t idx = findPyCandidateIndex(py);
    if (idx >= 0) {
        if (score > _pyScore[idx]) _pyScore[idx] = score;
        return;
    }

    if (_pyCount >= MAX_PY_CAND) return;

    _candPy[_pyCount] = py;
    _pyScore[_pyCount] = score;
    _pyCount++;
}


/************************新增***************************/

// 检查某个候选文字是否已经存在于当前候选列表中
// 用于避免重复候选

//新版2.0的candidateExists()函数
bool NineKeyIME::candidateExists(const char* hz) const {
    for (uint8_t i = 0; i < _hzCount; i++) {
        if (_candText[i] == hz) return true;
    }
    return false;
}




int8_t NineKeyIME::findCandidateIndex(const char* hz) const {
    for (uint8_t i = 0; i < _hzCount; i++) {
        if (_candText[i] == hz) return static_cast<int8_t>(i);
    }
    return -1;
}


int8_t NineKeyIME::findPyCandidateIndex(const char* py) const {
    for (uint8_t i = 0; i < _pyCount; i++) {
        if (_candPy[i] == py) return static_cast<int8_t>(i);
    }
    return -1;
}

//新版3.0addCandidate()函数
//默认参数只写在声明里，不写在定义里
void NineKeyIME::addCandidate(const char* py, const char* hz, int16_t score) {
    int8_t idx = findCandidateIndex(hz);
    if (idx >= 0) {
        if (score > _hzScore[idx]) _hzScore[idx] = score;
        return;
    }

    if (_hzCount >= MAX_HZ_CAND) return;

    _candText[_hzCount] = hz;
    _hzScore[_hzCount] = score;
    _hzCount++;
}



//3.0
void NineKeyIME::addUtf8CandidatesFromGroup(const char* py, const char* cands){
    const char* p = cands;
    int16_t order = 0;

    while (*p) {
        uint8_t n = utf8CharBytes((uint8_t)*p);

        char one[5] = {0};
        for(uint8_t i = 0; i < n; i++) {
            one[i] = p[i];
        }
        one[n] = '\0';

        String text = one;
        int16_t score = static_cast<int16_t>(1000 - order * 8 + userBonusFor(text) + contextBonusFor(text));
        addCandidate(py, one, score);

        p += n;
        order++;
    }
}



void NineKeyIME::addPhraseCandidatesFromGroup(const char* py, const char* cands, int16_t baseScore) {
    String phrase;
    int16_t order = 0;

    for (const char* p = cands; ; ++p) {
        if (*p == '|' || *p == '\0') {
            if (phrase.length() > 0) {
                int16_t score = static_cast<int16_t>(baseScore - order * 12 + userBonusFor(phrase) + contextBonusFor(phrase));
                addCandidate(py, phrase.c_str(), score);
                phrase = "";
                order++;
            }

            if (*p == '\0') break;
        } else {
            phrase += *p;
        }
    }
}




void NineKeyIME::rebuildCandidates() {
    clearCandidates();
    if (_digits.length() == 0) return;

    bool hasExact = false;

    for (size_t i = 0; i < KPhraseTableSize; i++) {
        String code = pinyinToDigits(KPhraseTable[i].py);
        if (code == _digits) {
            hasExact = true;
            addPyCandidate(KPhraseTable[i].py, static_cast<int16_t>(3200 - (i % 200)));
        }
    }

    for (size_t i = 0; i < KMainTableSize; i++) {
        String code = pinyinToDigits(KMainTable[i].py);
        if (code == _digits) {
            hasExact = true;
            addPyCandidate(KMainTable[i].py, static_cast<int16_t>(2200 - (i % 200)));
        }
    }

    for (size_t i = 0; i < KAliasTableSize; i++) {
        String code = pinyinToDigits(KAliasTable[i].py);
        if (code == _digits) {
            hasExact = true;
            addPyCandidate(KAliasTable[i].py, static_cast<int16_t>(1800 - (i % 200)));
        }
    }

    if (!hasExact) {
        for (size_t i = 0; i < KPhraseTableSize; i++) {
            String code = pinyinToDigits(KPhraseTable[i].py);
            if (code.startsWith(_digits)) {
                int16_t remain = static_cast<int16_t>(code.length() - _digits.length());
                addPyCandidate(KPhraseTable[i].py, static_cast<int16_t>(1400 - remain * 20 - (i % 50)));
            }
        }

        for (size_t i = 0; i < KMainTableSize; i++) {
            String code = pinyinToDigits(KMainTable[i].py);
            if (code.startsWith(_digits)) {
                int16_t remain = static_cast<int16_t>(code.length() - _digits.length());
                addPyCandidate(KMainTable[i].py, static_cast<int16_t>(1000 - remain * 12 - (i % 80)));
            }
        }

        for (size_t i = 0; i < KAliasTableSize; i++) {
            String code = pinyinToDigits(KAliasTable[i].py);
            if (code.startsWith(_digits)) {
                int16_t remain = static_cast<int16_t>(code.length() - _digits.length());
                addPyCandidate(KAliasTable[i].py, static_cast<int16_t>(800 - remain * 10 - (i % 80)));
            }
        }
    }

    if (_pyCount > 0) {
        sortPyCandidates();
        _pySel = 0;
        rebuildHanziFromSelectedPinyin();
    }
}



void NineKeyIME::rebuildHanziFromSelectedPinyin() {
    clearHanziCandidates();

    if (_pyCount == 0) return;
    if (_pySel >= _pyCount) _pySel = 0;

    const String selectedPy = _candPy[_pySel];

    for (size_t i = 0; i < KPhraseTableSize; i++) {
        if (selectedPy == KPhraseTable[i].py) {
            addPhraseCandidatesFromGroup(KPhraseTable[i].py, KPhraseTable[i].cands, static_cast<int16_t>(2200 - (i % 200)));
            sortHanziCandidates();
            return;
        }
    }

    for (size_t i = 0; i < KMainTableSize; i++) {
        if (selectedPy == KMainTable[i].py) {
            addUtf8CandidatesFromGroup(KMainTable[i].py, KMainTable[i].cands);
        }
    }

    for (size_t i = 0; i < KAliasTableSize; i++) {
        if (selectedPy == KAliasTable[i].py) {
            addUtf8CandidatesFromGroup(KAliasTable[i].py, KAliasTable[i].cands);
        }
    }

    sortHanziCandidates();
}



//3.0
void NineKeyIME::commitCurrentCandidate() {
    if (_hzCount == 0) return;

    String chosen = _candText[_hzSel];

    // 1. 把本次选中的字/词插入正文
    insertAtTextCursor(chosen);

    // 2. 更新“自学习频次表”
    touchUserFrequency(chosen);

    // 3. 记录“上一个已上屏词”，给下一轮上下文联想使用
    rememberCommittedText(chosen);

    // 4. 立刻写入 NVS，保证掉电不丢失
    //    这是本次最关键的增强点：ESP32 长期使用后会逐渐形成个人习惯。
    saveLearningToNvs();

    // 5. 清掉当前拼写状态
    clearComposition();

    Serial.print("commit = ");
    Serial.println(chosen);
}



// -------------------- 拼音转九键数字 --------------------

// 把一个字母映射到九键数字
char NineKeyIME::letterToDigit(char c) const {
    // 为了兼容大小写，先统一转成小写
    c = tolower((unsigned char)c);

    if (c >= 'a' && c <= 'c') return '2';
    if (c >= 'd' && c <= 'f') return '3';
    if (c >= 'g' && c <= 'i') return '4';
    if (c >= 'j' && c <= 'l') return '5';
    if (c >= 'm' && c <= 'o') return '6';
    if (c >= 'p' && c <= 's') return '7';
    if (c >= 't' && c <= 'v') return '8';
    if (c >= 'w' && c <= 'z') return '9';

    // 不属于 a~z 的字符统一返回 '0'
    // 当前实现里后续会忽略这些字符
    return '0';
}

// 把整段拼音转换成九键数字串
// 例如：
// "ni"     -> "64"
// "hao"    -> "426"
// "nihao"  -> "64426"
String NineKeyIME::pinyinToDigits(const char* py) const {
    String out;

    // *py 表示当前字符；当遇到字符串结束符 '\0' 时循环结束
    while (*py) {
        // 取当前字符，转换成数字，同时 py 指针后移一个字符
        char d = letterToDigit(*py++);                  //*py++先执行*再执行++

        // 只有合法映射结果才加入输出串
        if (d != '0') out += d;             //d != '0'这个判断对应上面的letterToDigit()函数，不合法的输入会返回数值0
    }
    return out;
}

// -------------------- 删除 UTF-8 最后一个字符 --------------------
// 这个函数用于安全删除中文字符。
// 因为 UTF-8 汉字通常占多个字节，不能直接删除最后一个字节，否则会乱码。
void NineKeyIME::deleteLastUtf8Char(String &s) {
    int len = s.length();
    if (len <= 0) return;

    // 从最后一个字节开始往前找，定位到最后一个 UTF-8 字符的起始字节
    int i = len - 1;

    // UTF-8 的续字节满足二进制形式 10xxxxxx
    // 判断方式： (byte & 0xC0) == 0x80
    // 如果当前是续字节，就继续向前找真正的起始字节
    while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) {
        i--;
    }

    // 从字符起始位置删除到末尾，相当于删除“最后一个完整字符”
    s.remove(i);
}


uint16_t NineKeyIME::utf8CharCount(const String &s) const{
        uint16_t count = 0;

        for(uint16_t i= 0; i< s.length();i++)
        {
            if(((uint8_t)s[i] & 0xC0) != 0x80)
            {
                count++;
            }
        }

        return count;
}

uint16_t NineKeyIME::utf8ByteOffset(const String &s, uint16_t charPos)  const
{
        uint16_t bytePos = 0;
        uint16_t charCount = 0;

        while(bytePos < s.length() && charCount < charPos)
        {
            bytePos++;
            while (bytePos < s.length() && ((uint8_t)s[bytePos] & 0XC0) == 0x80)
            {
                /* code */
                bytePos++;
            }
            charCount++;
        }
        return bytePos;
}


void NineKeyIME::moveTextCursorLeft(){
    
    // if(_textCursor = 0) _textCursor = _text.length();
    if(_textCursor > 0) _textCursor--;
    else    _textCursor = utf8CharCount(_text);

    // _textCursor = (_textCursor == 0) ? (utf8CharCount(_text)) : _textCursor--;
   
}


void NineKeyIME::moveTextCursorRight(){
    uint16_t totalChars = utf8CharCount(_text);
    if(_textCursor < totalChars)
        _textCursor++;
    else    _textCursor = 0;
}


void NineKeyIME::insertAtTextCursor(const String &ins) {
    uint16_t bytePos = utf8ByteOffset(_text, _textCursor);

    String left = _text.substring(0, bytePos);
    String right = _text.substring(bytePos);
    _text = left + ins + right;

    _textCursor += utf8CharCount(ins);
}


//3.0
void NineKeyIME::deleteBeforeTextCursor() {
    if (_textCursor == 0) return;

    uint16_t endByte = utf8ByteOffset(_text, _textCursor);
    uint16_t startByte = utf8ByteOffset(_text, _textCursor - 1);

    _text.remove(startByte, endByte - startByte);
    _textCursor--;

    // 用户手动删字后，原来的“上一词上下文”很可能已经不可靠，
    // 这里直接清空上下文，避免联想方向跑偏。
    _lastCommitted = "";
    saveLearningToNvs();
}


String NineKeyIME::textCursorPreview() const {
    uint16_t bytePos = utf8ByteOffset(_text, _textCursor);

    String s = _text.substring(0, bytePos);
    s += "|";
    s += _text.substring(bytePos);

    return s;
}



static String buildPreview(const String* list,
                           uint8_t count,
                           uint8_t sel,
                           uint8_t maxShow,
                           bool active)
{
    if (count == 0) return "无";
    if (maxShow == 0) return "";

    String s;

    uint8_t page = sel / maxShow;
    uint8_t start = page * maxShow;
    uint8_t end = start + maxShow;
    if (end > count) end = count;

    for (uint8_t i = start; i < end; i++) {
        if (i > start) s += " ";

        if (i == sel) {
            s += active ? "[" : "(";
            s += list[i];
            s += active ? "]" : ")";
        } else {
            s += list[i];
        }
    }

    return s;
}


String NineKeyIME::pyCandidatesPreview(uint8_t maxShow) const {
    return buildPreview(_candPy, _pyCount, _pySel, maxShow, _cnStage == CN_STAGE_PY);
}

String NineKeyIME::hzCandidatesPreview(uint8_t maxShow) const {
    return buildPreview(_candText, _hzCount, _hzSel, maxShow, _cnStage == CN_STAGE_HZ);
}

String NineKeyIME::candidatesPreview(uint8_t maxShow) const {
    return hzCandidatesPreview(maxShow);
}

String NineKeyIME::currentCandidate() const {
    if (_cnStage == CN_STAGE_HZ) {
        if (_hzCount == 0) return "";
        return _candText[_hzSel];
    }

    if (_pyCount == 0) return "";
    return _candPy[_pySel];
}

void NineKeyIME::setText(const String& s) {
    _text = s;
    _textCursor = utf8CharCount(_text);
}

//插入排序，这个可以深入了解一下。
void NineKeyIME::sortPyCandidates(){
        for(uint8_t i = 1; i < _pyCount; i++){
            String  py = _candPy[i];
            int16_t score = _pyScore[i];
            int j = i - 1;

            while (j >= 0 && _pyScore[j] < score){
                    _candPy[j + 1] = _candPy[j];
                    _pyScore[j + 1] = _pyScore[j];
                    j--;
            }
                _candPy[j + 1] = py;
                _pyScore[j + 1] = score;
        }
}


void NineKeyIME::sortHanziCandidates() {
    for (uint8_t i = 1; i < _hzCount; i++) {
        String text = _candText[i];
        int16_t score = _hzScore[i];
        int j = i - 1;

        while (j >= 0 && _hzScore[j] < score) {
            _candText[j + 1] = _candText[j];
            _hzScore[j + 1] = _hzScore[j];
            j--;
        }

        _candText[j + 1] = text;
        _hzScore[j + 1] = score;
    }
}

//查询这个候选的用户频次加分
int16_t NineKeyIME::userBonusFor(const String& text) const{
            for(uint8_t i = 0; i < _userFreqCount; i++){
                    if(_userFreq[i].text == text){
                        uint16_t freq = _userFreq[i].freq;
                        int16_t bonus = static_cast<int16_t>(freq * 40);    //可以查一下这个的用法
                        return  bonus > 800 ? 800 :  bonus;
                    }
            }
            return 0;
}

//查询这个候选的上下文加分
//这个代码可以看一下，并没有好好看过
int16_t NineKeyIME::contextBonusFor(const String& text) const{
        if (_lastCommitted.length() == 0) return 0;

        int16_t best = 0;
        for (size_t i = 0; i < KContextTableSize; i++) {
            if (_lastCommitted == KContextTable[i].prev) {
                String next = KContextTable[i].next;
                if (text == next) {
                    if (KContextTable[i].bonus > best) best = KContextTable[i].bonus;
                } else if (text.startsWith(next)) {
                    int16_t half = KContextTable[i].bonus / 2;
                    if (half > best) best = half;
                }
            }
        }
        return best;
}


void NineKeyIME::rememberCommittedText(const String& text){
        _lastCommitted = text;
}


void NineKeyIME::touchUserFrequency(const String& text){
            if (text.length() == 0) return;

    for (uint8_t i = 0; i < _userFreqCount; i++) {
        if (_userFreq[i].text == text) {
            if (_userFreq[i].freq < 65535) _userFreq[i].freq++;
            return;
        }
    }

    if (_userFreqCount < MAX_USER_FREQ) {
        _userFreq[_userFreqCount].text = text;
        _userFreq[_userFreqCount].freq = 1;
        _userFreqCount++;
        return;
    }

    uint8_t minIdx = 0;
    for (uint8_t i = 1; i < MAX_USER_FREQ; i++) {
        if (_userFreq[i].freq < _userFreq[minIdx].freq) minIdx = i;
    }
    _userFreq[minIdx].text = text;
    _userFreq[minIdx].freq = 1;
}