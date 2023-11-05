#include <gccore.h>
#include <stdio.h>
#include <unistd.h>
#include <wiilight.h>
#include <wiiuse/wpad.h>

#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "fat.h"
#include "IOSCheck.h"
#include "IOSPatcher.h"
#include "IOSMenu.h"
#include "nand.h"
#include "Settings.h"
#include "title_install.h"
#include "tools.h"
#include "wad.h"
#include "video.h"

int yes_no_checkeach(void);
int installIOS(int iosloc);
int tryInstallIOS(int found2);
int doparIos3(u32 ver, u32 rev, u32 loc);
int doparIos(u32 ver, u32 rev, u32 loc, int sigPatch, int esIdentPatch,
    int applynandPatch, int chooseloc);
u8 choosebaseios(u8 cur);

const IOSlisting GrandIOSList[] = {
    { 0, 0, "\n", 1, {
            { 65280, IOS_NoNusstub, { 0x0, 0x0, 0x0, 0x0, 0x0 } } } },
    { 3, 0, "Non-functional stub.\n", 1, {
        { 65280, IOS_NoNusstub, { 0x9e3ecc3f, 0x40e86648, 0xf387b9a8, 0xd24e9bed, 0xded4c651 } } } },
    { 4, 1, "Stub, useless now.\n", 3, {
        { 3, IOS_NotonNus, { 0x362b813a, 0x4bfc1940, 0x57608a9e, 0xc4e52af8, 0x4731eb6 } },
        { 259, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 65280, IOS_Stub, { 0xe465142b, 0x57955f7d, 0xa52617b9, 0xf79a0067, 0xd8c943b6 } } } },
    { 5, 0, "Non-functional stub.\n", 1, {
        { 65280, IOS_NoNusstub, { 0x0, 0x0, 0x0, 0x0, 0x0 } } } },
    { 9, 2, "Used by launch titles (IE: Zelda: Twilight Princess, Wii Sports)\nand System Menu 1.0.", 7, {
        { 516, IOS_NotonNus, { 0xaddab4be, 0xcbad9281, 0xfa299a94, 0x811f625a, 0x203599b0 } },
        { 518, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 520, IOS_OK, { 0x45819b89, 0x25cf22e6, 0xbd9cafa7, 0x2c269e62, 0x897cb125 } },
        { 521, IOS_OK, { 0x7238a3a1, 0xe277ed95, 0xaafa553e, 0x4f516575, 0xbe01e47 } },
        { 778, IOS_OK, { 0xbc090d25, 0x4bcf7202, 0x8fea141b, 0x75ff0f84, 0x64d27e90 } },
        { 1034, IOS_OK, { 0xc6ab909b, 0x6841bbd5, 0x75f50b75, 0x6a33c695, 0x9e03b52 } },
        { 1290, IOS_NotonNus, { 0xbb76b669, 0x5ac00a93, 0xb5d18f06, 0x8e753947, 0xe26c8c10 } } } },
    { 10, 1, "Stub, useless now.\n", 1, {
        { 768, IOS_Stub, { 0x59c468b0, 0x73874de3, 0x25f58286, 0x5e067bbe, 0xc82de307 } } } },
    { 11, 2, "Used only by System Menu 2.0 and 2.1.\n", 2, {
        { 10, IOS_OK, { 0x4483caa8, 0x9f951f17, 0x1931aee0, 0xb4ce4df5, 0x5cf692d } },
        { 256, IOS_Stub, { 0x6eeef316, 0xfb936458, 0xed05d886, 0xfe76155c, 0xfa5791cf } } } },
    { 12, 2, "\n", 7, {
        { 6, IOS_OK, { 0x6e1e10bd, 0xf31556dc, 0xff079ceb, 0xcdb66dbc, 0xc600cc66 } },
        { 11, IOS_OK, { 0x1eda9d6e, 0x64573448, 0x68df0fb1, 0x6ad29af3, 0x6b7d8341 } },
        { 12, IOS_OK, { 0x16443da0, 0xe8fc9914, 0xa7fbd0a0, 0xdcf26383, 0x1537e47c } },
        { 269, IOS_OK, { 0xa051cbe0, 0x60f95840, 0x6b3a8693, 0x5cccbc17, 0xad31ccf9 } },
        { 525, IOS_OK, { 0xfb3f2c1e, 0xa5320ed4, 0xb1d21ac7, 0xc77d00a7, 0x2ac511d5 } },
        { 526, IOS_OK, { 0x552f62e7, 0x987311f9, 0xef663145, 0xe4473d2a, 0x9639718a } },
        { 782, IOS_NotonNus, { 0x515fc55e, 0xe1beca47, 0x8a79eb98, 0x11ede266, 0xf159dbde } } } },
    { 13, 2, "Used by the 'All Regions' titles for News, Weather, & Photo 1.0 Channels.\n", 7, {
        { 10, IOS_OK, { 0xcde62038, 0xca4790bd, 0xd25dcd7c, 0x3e046064, 0xa807527a } },
        { 15, IOS_OK, { 0x7eabaebf, 0xa57496a1, 0x929020be, 0xc641b931, 0x91cec719 } },
        { 16, IOS_OK, { 0x3dab2795, 0x23d1db9b, 0x25967e6a, 0xaa7109a6, 0x7c9d1a5c } },
        { 273, IOS_OK, { 0xfe7897b6, 0x9db43fca, 0xdda408d6, 0xe9ce47c2, 0x6ea23d5 } },
        { 1031, IOS_OK, { 0x9c22eac3, 0x1df31c06, 0xad0f1f8d, 0xbb0708f4, 0x754fcb5c } },
        { 1032, IOS_OK, { 0x100cc671, 0xa6ce3c5f, 0xfe599047, 0x6326b976, 0x6f43fe2e } },
        { 1288, IOS_NotonNus, { 0x6aa8ce69, 0x9bb1e00d, 0x8b475342, 0x7de650e2, 0xd9682101 } } } },
    { 14, 2, "\n", 7, {
        { 257, IOS_NotonNus, { 0x314ef52f, 0xbf7bcac3, 0x7632ca2b, 0x1a74979, 0xee2965b6 } },
        { 262, IOS_OK, { 0xdb7cc4d5, 0xd8cf4c53, 0x577c0374, 0x933571c, 0xe657a0b } },
        { 263, IOS_OK, { 0x2cdc4953, 0xa0f10dae, 0x909621d, 0x3a3279f8, 0x19671ec1 } },
        { 520, IOS_OK, { 0xac398f4, 0x68f0f2ba, 0xf4d377c6, 0xe2e6db36, 0x26008b9c } },
        { 1031, IOS_OK, { 0xe02af898, 0xc9e117bd, 0x268d7de6, 0x6ba0dfb6, 0x51869d5b } },
        { 1032, IOS_OK, { 0x16af1a5a, 0xa771e53b, 0x9173f7ff, 0xe53e3ae0, 0x6dcbf762 } },
        { 1288, IOS_NotonNus, { 0xbf89e80e, 0xaad1691d, 0x4d4d4e77, 0x8ded10ba, 0xa01f9253 } } } },
    { 15, 2, "\n", 10, {
        { 257, IOS_OK, { 0xd6127cb9, 0xbbda03a4, 0x3f4c9b13, 0x120298bb, 0xbf2b314a } },
        { 258, IOS_OK, { 0x9d9c04be, 0x350be3f7, 0xa496e522, 0x10920b9a, 0xebd23d68 } },
        { 259, IOS_OK, { 0x2b8ec63d, 0x13cab9ce, 0xfc865f20, 0xc7521b71, 0x895dfa5d } },
        { 260, IOS_OK, { 0x169980ed, 0xd5f69c9c, 0xeb82bc65, 0xb2f42636, 0x8520ceb } },
        { 265, IOS_OK, { 0xaffec52c, 0xb9f029ff, 0xafde816d, 0xafadffaa, 0xe9f81534 } },
        { 266, IOS_OK, { 0xa94fb180, 0xb13afd73, 0x26fe3c7c, 0xd398e1a, 0x32717677 } },
        { 523, IOS_OK, { 0x213f0a20, 0xb56fec5e, 0x38ecb53f, 0xfafe5fd5, 0xba2a8f4d } },
        { 1031, IOS_OK, { 0x890bc66e, 0xa43f9c81, 0x15f87f, 0xc12061b5, 0x2c1bafb8 } },
        { 1032, IOS_OK, { 0xc04e9104, 0x917f56d6, 0xcac074cb, 0xe0a76f24, 0x10d930e6 } },
        { 1288, IOS_NotonNus, { 0x67b8e9f5, 0x60ad2794, 0xecd5c39f, 0xdca60d37, 0xc1b46d94 } } } },
    { 16, 1, "Piracy prevention stub, useless.\n", 2, {
        { 257, IOS_NotonNus, { 0x5cd05b7, 0xbb856f76, 0xea6cf7dd, 0xac34268c, 0x4d574e74 } },
        { 512, IOS_Stub, { 0x3f73c30a, 0xd85ab73b, 0xa4b84b85, 0x63534fc2, 0x88c2be5d } } } },
    { 17, 2, "\n", 7, {
        { 512, IOS_OK, { 0x5045cad, 0x5e514b11, 0x2dfba9f0, 0xd6eb0d4e, 0xa1a917e6 } },
        { 517, IOS_OK, { 0xfcd500f6, 0xc25c8a40, 0x1b3f699f, 0xee1e4e7c, 0xe2e9517 } },
        { 518, IOS_OK, { 0xda8f8661, 0x5f879c82, 0xf1e33281, 0x470ae0e3, 0x3759745c } },
        { 775, IOS_OK, { 0xb46f1bef, 0xd77e9c7e, 0xa7595bc8, 0xa13e9585, 0x12302728 } },
        { 1031, IOS_OK, { 0x176cc677, 0xbb82be2b, 0xe6f93899, 0x584d04c0, 0xec1c0b95 } },
        { 1032, IOS_OK, { 0xff4bcce1, 0x5a7138af, 0xc57ddbec, 0xb99fc847, 0x51b8fcfb } },
        { 1288, IOS_NotonNus, { 0xf6e385d7, 0xcf8cdc89, 0x6c5331a3, 0xaea17826, 0x624461c2 } } } },
    { 20, 2, "Used only by System Menu 2.2.\n", 2, {
        { 12, IOS_OK, { 0x50b89b9a, 0x3139d181, 0x25f4c3c4, 0xf87da9d2, 0xc1f3c710 } },
        { 256, IOS_Stub, { 0x589af879, 0x5fbadeef, 0x7afb1057, 0xe4f7e968, 0x3d3d730d } } } },
    { 21, 2, "Used by old third-party titles (No More Heroes).\n", 10, {
        { 514, IOS_OK, { 0x6edbc139, 0x3d689833, 0xa1f84f50, 0x8b4a5a07, 0xb1592133 } },
        { 515, IOS_OK, { 0xdde5ea6a, 0x6898c3e0, 0x57cb3053, 0x520a2035, 0x7a072fb4 } },
        { 516, IOS_OK, { 0x862a2961, 0x1a52a056, 0x211c758, 0x15bc0929, 0x4a31af9 } },
        { 517, IOS_OK, { 0xe8252681, 0x6379fb15, 0x8ddfadae, 0x4f1fd1ac, 0x69768333 } },
        { 522, IOS_OK, { 0x3e3a9022, 0xa3e1c51, 0x9ec2463f, 0x11405762, 0x1036a4eb } },
        { 525, IOS_OK, { 0x95619f60, 0xf7b83cb2, 0x49d5cda8, 0x9bfacccc, 0xc7fa73cf } },
        { 782, IOS_OK, { 0xd287468a, 0x80b8aaae, 0xecec46e8, 0xf5b864d3, 0xe29753ed } },
        { 1038, IOS_OK, { 0x146d9e1a, 0x334c4fbf, 0xc18cf2b6, 0x27f9a177, 0xdd83b92d } },
        { 1039, IOS_OK, { 0x804937f7, 0xd3d49ee4, 0x9828864e, 0x46e8c0d5, 0xcfc82b68 } },
        { 1295, IOS_NotonNus, { 0xcd1aa602, 0x1b0f1885, 0xf4005a93, 0x7c6fe307, 0x6e0fe765 } } } },
    { 22, 2, "\n", 6, {
        { 777, IOS_OK, { 0x2fe6fb96, 0x69ac7ec5, 0x424efd33, 0x1ee622fc, 0x4ee5d174 } },
        { 780, IOS_OK, { 0x1ac5153, 0x562b1ccd, 0x186856a8, 0x61b5e846, 0x41aa360b } },
        { 1037, IOS_OK, { 0x3e8f258c, 0x59c9cd4f, 0xce32394c, 0x964c1190, 0xb9823420 } },
        { 1293, IOS_OK, { 0xf895d87d, 0x4415db7c, 0xdde269d6, 0x1f567446, 0xdc41704e } },
        { 1294, IOS_OK, { 0x215b1432, 0x82b912b2, 0xa96a2c26, 0x29ffb6cf, 0xd23b2a63 } },
        { 1550, IOS_NotonNus, { 0x9b48c5fb, 0xdf19e6b, 0x8a5a1f6b, 0xea58f53c, 0xfbf74078 } } } },
    { 28, 2, "\n", 6, {
        { 1292, IOS_OK, { 0xac077bc0, 0xe36460b4, 0xded0f72d, 0x987e270e, 0x78ad48db } },
        { 1293, IOS_OK, { 0xa95ae403, 0xbb1959c5, 0xc3250bb7, 0x6bb2d86e, 0x21290559 } },
        { 1550, IOS_OK, { 0xab16fa66, 0x2de6e4d7, 0xdf92edb6, 0x591beed9, 0x85b47c28 } },
        { 1806, IOS_OK, { 0xa869658, 0xf5901c9e, 0x47c4c3ba, 0x585b399d, 0x3fbd72f8 } },
        { 1807, IOS_OK, { 0x247bf6d, 0xd732bc66, 0x24c15d1d, 0x646e17f0, 0xd76b94c4 } },
        { 2063, IOS_NotonNus, { 0x133f9a8, 0x5e05d4a8, 0x713a5406, 0xb30cb1fd, 0x30f164e2 } } } },
    { 30, 2, "Used only by System Menu 3.0, 3.1, 3.2 and 3.3.\n", 5, {
        { 1037, IOS_OK, { 0xaba62898, 0x87da30a2, 0x108800dc, 0xbf46ebe6, 0xe3432adf } },
        { 1039, IOS_OK, { 0x52922816, 0xded2ba55, 0x232813ca, 0x1f5ddd0d, 0xd693fae4 } },
        { 1040, IOS_OK, { 0x997befd1, 0x8c0e513e, 0xadd67a50, 0x294b3efb, 0x6fbb7c7f } },
        { 2576, IOS_OK, { 0xba8e4a2, 0x7c31dbf8, 0xc2bbe936, 0xaf0a0600, 0x9ad43de8 } },
        { 2816, IOS_Stub, { 0x5fd69cdf, 0x8197c405, 0xa02a8419, 0xae1aaf0f, 0xd9c09190 } } } },
    { 31, 2, "Used by the Regional titles for News/Weather/Photo 1.1 Channel\n", 10, {
        { 1037, IOS_OK, { 0x7dff2e68, 0x2a972f39, 0x28c3148b, 0x241bb41c, 0xee6a6bfc } },
        { 1039, IOS_OK, { 0x14fe8875, 0x5c7d8257, 0x220562cc, 0x79826748, 0xb58a3d93 } },
        { 1040, IOS_OK, { 0x9794f914, 0x7cf5b5, 0x794cf39a, 0x452df384, 0x615cfd0c } },
        { 2576, IOS_OK, { 0x8050a640, 0x39307fc2, 0xd7376920, 0x4cd74117, 0xc0073d45 } },
        { 3088, IOS_OK, { 0x10341ebe, 0x24ac2a92, 0x67ea3dc, 0xcf94a290, 0xeaa7ab6f } },
        { 3092, IOS_OK, { 0xf6a1f54, 0x9d31ecd5, 0xf46bbb91, 0x6649399d, 0x5cc237cb } },
        { 3349, IOS_OK, { 0x15d666b6, 0x947125df, 0xc03bb237, 0x2851ac04, 0xfed6123 } },
        { 3607, IOS_OK, { 0x37ba3a79, 0x429567e6, 0xc6652151, 0xb3eb46d, 0xa0e7de37 } },
        { 3608, IOS_OK, { 0xa81677d5, 0xe96cc79c, 0xb87aeda3, 0x8e05591a, 0x72203d37 } },
        { 3864, IOS_NotonNus, { 0x2f7531d5, 0x6f52d4ba, 0x72b44aa8, 0x731a2e4, 0xa152dc22 } } } },
    { 33, 2, "\n", 7, {
        { 1040, IOS_OK, { 0x1ad9a3a3, 0xfcd34f94, 0x22aec35d, 0xa58cf181, 0xe32da94c } },
        { 2832, IOS_OK, { 0xc5ecdcd2, 0x7bdf8596, 0x61e1ebc0, 0x454548e, 0x353130be } },
        { 2834, IOS_OK, { 0x4de821e3, 0xcdd0e87d, 0xb32ae62a, 0xc25ceb35, 0x59408482 } },
        { 3091, IOS_OK, { 0x377582f6, 0xb25e431b, 0x31d37d20, 0xe0c9d550, 0x604288e6 } },
        { 3607, IOS_OK, { 0x7cafd0ba, 0x880093df, 0xe9b89a1e, 0x1e3f397c, 0x1feb175d } },
        { 3608, IOS_OK, { 0xf749b24b, 0xa847ac68, 0x16aaed02, 0x7fabb3b4, 0x94ec8e80 } },
        { 3864, IOS_NotonNus, { 0x7259871f, 0x8e8a927a, 0x78cb209, 0x5e087dae, 0x8d446718 } } } },
    { 34, 2, "\n", 7, {
        { 1039, IOS_OK, { 0x6af73c49, 0xf031069e, 0xb10ba9b5, 0x9cc4569d, 0x3c34b1e } },
        { 3087, IOS_OK, { 0x99452f4e, 0xee235555, 0xc3b11b97, 0x93cfe78c, 0x8027f0ee } },
        { 3091, IOS_OK, { 0xbfc16205, 0xd97d6cc8, 0x91398359, 0xa27c7aaa, 0x5f37933c } },
        { 3348, IOS_OK, { 0x5010cf4f, 0x82101363, 0xcae31c46, 0xd7358e6d, 0x7ac84030 } },
        { 3607, IOS_OK, { 0xb2c134d0, 0xac0ec45b, 0xe11769a1, 0x20de1d11, 0xb30105a1 } },
        { 3608, IOS_OK, { 0xa53c53f2, 0xd5520ff4, 0xa8260529, 0xe7d079c5, 0x85cdb27e } },
        { 3864, IOS_NotonNus, { 0xe22b9030, 0x4f6ea499, 0x81e1092c, 0x58481c05, 0x1c74bbec } } } },
    { 35, 2, "Used by various games including Super Mario Galaxy, Call Of Duty, Wii Music.\n", 7, {
        { 1040, IOS_OK, { 0xf7fe5127, 0x3e64e1b0, 0xb20b71af, 0xf6935311, 0x2a4c1ea8 } },
        { 3088, IOS_OK, { 0xcebcb283, 0x453d591e, 0x6df15c06, 0x49eb4d52, 0x4f15376f } },
        { 3092, IOS_OK, { 0x40de122c, 0x94e53134, 0x8e1b02dd, 0x2d1743a0, 0xe83e188f } },
        { 3349, IOS_OK, { 0x1152f20b, 0x854cbe4a, 0x4b0a97db, 0xf5e73567, 0xed305cdd } },
        { 3607, IOS_OK, { 0x83675f7e, 0xdd820e14, 0x5af87ac0, 0xb4eae2ad, 0xd4ea0b96 } },
        { 3608, IOS_OK, { 0x8fd25f2d, 0xe29a52cb, 0xcf9d6bc1, 0xd4ebec8d, 0xd3871228 } },
        { 3864, IOS_NotonNus, { 0xc4fad12, 0x2d0786d2, 0x7b5d9819, 0x27f5cb67, 0x9cde7372 } } } },
    { 36, 2, "Used by: Smash Bros. Brawl, Mario Kart Wii.\nCan be ES_Identify patched.", 7, {
        { 1042, IOS_OK, { 0x771c6a91, 0x24996063, 0x2ca7321c, 0xe16a03e2, 0x3a5f061c } },
        { 3090, IOS_OK, { 0x76875485, 0x3c8fa2d1, 0x46d34da6, 0x3fb02c7f, 0x6bf575cc } },
        { 3094, IOS_OK, { 0x1b91560a, 0x1b7aaa7f, 0x92c610bb, 0x4dce2f2, 0xc00651c5 } },
        { 3351, IOS_OK, { 0xbb1bc80, 0x518b0147, 0x3a6aea03, 0x7fa361b6, 0xff5b10a1 } },
        { 3607, IOS_OK, { 0x4e5371ce, 0xe438e6bb, 0xef5b8ffb, 0xe629aa21, 0xafe170fe } },
        { 3608, IOS_OK, { 0xcf561d25, 0x854da7db, 0xbd18433f, 0x5f51253, 0x3ce24db3 } },
        { 3864, IOS_NotonNus, { 0x29ef9a1f, 0xe1da8bbb, 0x4b142090, 0x851d80ae, 0x4d9d06f2 } } } },
    { 37, 2, "Used mostly by music games like Guitar/Band Hero and Rock Band.\n", 7, {
        { 2070, IOS_OK, { 0xdc448cc3, 0xf7ba2523, 0x8ee58566, 0x6b980a1d, 0xb05a8d4f } },
        { 3609, IOS_OK, { 0x88b3b49, 0x400fc839, 0xe80af646, 0x21b21128, 0x9c3cacb8 } },
        { 3612, IOS_OK, { 0x7a8bae2, 0xabae2e5, 0x4421b7e8, 0x9a0c7825, 0xece74f4b } },
        { 3869, IOS_OK, { 0x4105d93f, 0x71ddf875, 0xf51acd88, 0xcab57940, 0x785bea8f } },
        { 5662, IOS_OK, { 0xe0079f75, 0xecfc6ab, 0xc2f9611c, 0x85ba6c75, 0xbf6ae9d9 } },
        { 5663, IOS_OK, { 0xdb6e64e9, 0xb6ee7695, 0xee5859c1, 0x5618fda8, 0xfe805611 } },
        { 5919, IOS_NotonNus, { 0xce155728, 0x4f6372f4, 0xdcea3fad, 0xe936a0bb, 0x921e2f26 } } } },
    { 38, 2, "Used by some modern titles (Animal Crossing).\n", 6, {
        { 3609, IOS_NotonNus, { 0x6e77bdaf, 0xa07df24f, 0x4beb2852, 0xe774b646, 0xdc096e5d } },
        { 3610, IOS_OK, { 0xfec3e30d, 0xf184e5d7, 0xbf32fbbc, 0xb7baabca, 0x3bec0bdb } },
        { 3867, IOS_OK, { 0xa30dd2c6, 0x57258f56, 0x65409d16, 0x87f0f304, 0xf9ff6f60 } },
        { 4123, IOS_OK, { 0x6e2f3314, 0x4a5678a, 0xd45ebfaf, 0x90f772ad, 0x3c0587d8 } },
        { 4124, IOS_OK, { 0x2ab93bf4, 0xc29744c2, 0xcb7fd224, 0x70cceaed, 0xe1937973 } },
        { 4380, IOS_NotonNus, { 0xa6e42acd, 0xbbb4bcb, 0x513767af, 0xbd0c2f3d, 0x5fc12203 } } } },
    { 40, 1, "Present in Korean system.\n", 3, {
        { 2321, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 2835, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 3072, IOS_Stub, { 0x12edf6e1, 0x7f49d4f9, 0xd2df5259, 0x52c79545, 0x57a028e4 } } } },
    { 41, 2, "Present in Korean system.\n", 6, {
        { 2320, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 2835, IOS_OK, { 0xe45c79e9, 0x894fde1a, 0xec3d77f8, 0x73c0e576, 0x74a46025 } },
        { 3091, IOS_OK, { 0xb7a3dd3a, 0xe6b5c3b0, 0x75c53c20, 0xa2bb5fcf, 0xc46b496f } },
        { 3348, IOS_OK, { 0x3cf0a1bd, 0x11b0150, 0x43896ebc, 0x99b894f6, 0x9f1fbec9 } },
        { 3606, IOS_OK, { 0x8a0a18e1, 0xd00ae208, 0xc3b7f4fe, 0x923c886b, 0xb182dbb7 } },
        { 3607, IOS_OK, { 0xe0dafeb7, 0x745810b4, 0xcd2d180d, 0xc820f16d, 0x931a05ab } } } },
    { 43, 2, "Present in Korean system.\n", 6, {
        { 2320, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 2835, IOS_OK, { 0xe45c79e9, 0x894fde1a, 0xec3d77f8, 0x73c0e576, 0x74a46025 } },
        { 3091, IOS_OK, { 0x8a02d216, 0x9e711529, 0xb4a8411b, 0x267d4eb3, 0xb17efe3e } },
        { 3348, IOS_OK, { 0x3fcf8efc, 0xb1be57d3, 0x6de586d3, 0x8a59ca24, 0x6467d828 } },
        { 3606, IOS_OK, { 0xceff8841, 0x2bcf150a, 0xa03a6b59, 0xf6bfb8eb, 0x77d61273 } },
        { 3607, IOS_OK, { 0x7b263012, 0x7e4c1112, 0xe89f07f3, 0xfe467d0c, 0xf0e2a021 } } } },
    { 45, 2, "Present in Korean system.\n", 6, {
        { 2320, IOS_NotonNus, { 0x0, 0x0, 0x0, 0x0, 0x0 } },
        { 2835, IOS_OK, { 0x97ce8fe6, 0x7825c4a1, 0x692f0249, 0xe027f796, 0x49cd33dd } },
        { 3091, IOS_OK, { 0xa20c902f, 0x89d21eb3, 0x92467248, 0xd549165e, 0xbdf8f45e } },
        { 3348, IOS_OK, { 0xafbf85ec, 0xdd9db6c8, 0x265c765e, 0x88bb4654, 0xaba743f0 } },
        { 3606, IOS_OK, { 0x8902de7, 0x54258e1f, 0x14ed01b9, 0xf25bb1f2, 0x4a2f5b90 } },
        { 3607, IOS_OK, { 0x2e4170da, 0x3188ba67, 0x88f61023, 0x1765e8fc, 0x31c66c2c } } } },
    { 46, 2, "Present in \"Need for Speed Undercover\" and in \"Shin Chuukadaisen\" Korean.\n", 6, {
        { 2322, IOS_NotonNus, { 0xbda81fd9, 0xdcc263f5, 0x324b025, 0xa2230f87, 0x9fb75903 } },
        { 2837, IOS_OK, { 0x5bfe455, 0x7c896a15, 0xd5b2139b, 0xe2a09de, 0x71e6b43e } },
        { 3093, IOS_OK, { 0xb39e1e8c, 0xf83b86f0, 0x5238cae4, 0x3c56eae8, 0xafe0d9a5 } },
        { 3350, IOS_OK, { 0x133d9a4b, 0x7d20544b, 0x2008b4d8, 0xb6ad8e8b, 0x957a10be } },
        { 3606, IOS_OK, { 0xe62d2203, 0xb2cdcadd, 0x765fb119, 0xeb94492d, 0x98c7edf0 } },
        { 3607, IOS_OK, { 0xcaf1c435, 0x61fa3ba0, 0xec289f26, 0xca0c8a15, 0xba17243e } } } },
    { 48, 2, "Bundled with System Menu 4.3K provides the same features as IOS38 but has a new SDI module and an FFSP vs FFS module.", 2, {
        { 4123, IOS_OK, { 0xe2cc2050, 0xe0804742, 0x26600e1d, 0x7b49c7fa, 0xa8ca6d64 } },
        { 4124, IOS_OK, { 0xd24dc44a, 0x5c4d9733, 0x6c599660, 0x93038d38, 0x4308b129 } } } },
    { 50, 2, "Used only by System Menu 3.4.\n", 2, {
        { 4889, IOS_OK, { 0x9340fd98, 0xa25133b6, 0xe314ce6b, 0xa729257f, 0x56b17f85 } },
        { 5120, IOS_Stub, { 0xb21c7e5a, 0x16be62e3, 0xb629f0cb, 0x10821951, 0x7eb63057 } } } },
    { 51, 2, "Used only by System Menu 3.4.\n", 2, {
        { 4633, IOS_OK, { 0x9119ded2, 0x4b5e34da, 0x6d4d779, 0x68b087a9, 0xf2f4ce1f } },
        { 4864, IOS_Stub, { 0x3c068202, 0x2006c1b6, 0x40a0c82f, 0x5bd0436b, 0x9d343e9f } } } },
    { 52, 2, "Used by System Menu 3.5 (Korea only)\n", 2, {
        { 5661, IOS_OK, { 0xf04f3ab9, 0xe687f9, 0x34e34f82, 0x998e681e, 0xae05761e } },
        { 5888, IOS_Stub, { 0xd72a8362, 0x7a40c485, 0x4e27d812, 0x720d49f1, 0xef2b4a4f } } } },
    { 53, 2, "Required for many WiiWare titles.\nv5406 required for New Super Mario Brothers.", 6, {
        { 4113, IOS_OK, { 0xd0a3df4, 0xf842d77f, 0xb8b0c705, 0x5ab7b75a, 0xa3065b95 } },
        { 5149, IOS_OK, { 0xc38519cf, 0x7e117f66, 0x6f64089b, 0x8be586da, 0x9a95daf2 } },
        { 5406, IOS_OK, { 0x8add9b1d, 0xea16d4ff, 0x3ba2b5ad, 0xc648006, 0x95aa8a87 } },
        { 5662, IOS_OK, { 0xee4b0665, 0x26012e08, 0x9fdf46b3, 0xdc6cfb59, 0x84939d2d } },
        { 5663, IOS_OK, { 0xdffa80d3, 0x3812b69c, 0x5d5b12c5, 0x7d51882f, 0x45e8aea5 } },
        { 5919, IOS_NotonNus, { 0x44a3665c, 0xfaf03022, 0x426a1970, 0x442a8b9a, 0xbb7e0ea1 } } } },
    { 55, 2, "Used by some modern games and channels and some Wiiware titles.\n", 6, {
        { 4633, IOS_OK, { 0x2cb27ab1, 0x543cb709, 0xc0d39885, 0xe1b04eee, 0xbb617674 } },
        { 5149, IOS_OK, { 0xd574ada6, 0x42f13ec3, 0xdac2854d, 0xbc47026e, 0x6f55718a } },
        { 5406, IOS_OK, { 0xd8cc1572, 0x4c118341, 0x3115261f, 0xd4575b5, 0x34b58b41 } },
        { 5662, IOS_OK, { 0xe2d6abc5, 0x1b51d0ed, 0x58c0f3d2, 0xd733c239, 0xcd8d94dd } },
        { 5663, IOS_OK, { 0xe4615432, 0x226e879f, 0xe10ff10a, 0xe5dfb6ca, 0x91d7a2e5 } },
        { 5919, IOS_NotonNus, { 0xbbff636e, 0x1be2837f, 0xe555d7a7, 0xe2c17c05, 0x52561b9b } } } },
    { 56, 2, "Used by Wii Speak Channel and some newer WiiWare.\n", 6, {
        { 4890, IOS_OK, { 0x17b560ce, 0xe00c8b5, 0x502818ac, 0x9557e752, 0x178d7f71 } },
        { 5146, IOS_NotonNus, { 0x71e797cb, 0xcd074a9, 0x9ebb3b06, 0xc58e9bb, 0x38ba6830 } },
        { 5405, IOS_OK, { 0xe84086fb, 0x81ac2465, 0xe3d2b274, 0xb4f129b5, 0x588a659 } },
        { 5661, IOS_OK, { 0x5fd52dab, 0xe19eafb5, 0x7b7ec433, 0x619768a8, 0xf6432704 } },
        { 5662, IOS_OK, { 0x1f215272, 0xbc788dee, 0x31367960, 0x2ed75c30, 0xa1775c43 } },
        { 5918, IOS_NotonNus, { 0x15356222, 0xf0f1e9f2, 0x979d388d, 0x7fc3af07, 0x40aaf854 } } } },
    { 57, 2, "Contains some new USB & Ethernet Modules, Unknown usage at this time.\n", 5, {
        { 5404, IOS_OK, { 0x5b4e8539, 0xfad1c8e6, 0xc3f75516, 0x7369bf6c, 0x3a8caeb1 } },
        { 5661, IOS_OK, { 0x6ef1f046, 0xdf1f128f, 0x5f17f537, 0x59cb8568, 0xf6d1de65 } },
        { 5918, IOS_OK, { 0x3c5bcf91, 0x64a03b36, 0xb00841ea, 0x32538131, 0xbb47ede6 } },
        { 5919, IOS_OK, { 0x8645c24c, 0x5967ef92, 0xb941eab6, 0x6a5ab458, 0x8e36389a } },
        { 6175, IOS_NotonNus, { 0xc4ece7af, 0x5bef69aa, 0xce4dd892, 0x6400994a, 0x3031b95f } } } },
    { 58, 2, "Comes with the game \"Your Shape\" to allow the USB camera to work\n", 4, {
        { 5918, IOS_NotonNus, { 0xfd7274f1, 0xb2621163, 0x97c7e65b, 0x22755cef, 0x1e64d7ac } },
        { 6175, IOS_OK, { 0xc2cd8958, 0x9174385a, 0x387894a5, 0x8034b601, 0x3a294a5b } },
        { 6176, IOS_OK, { 0x3405b5e9, 0xcef169f5, 0x12451f03, 0xdda41b6a, 0xb77fa632 } },
        { 6432, IOS_NotonNus, { 0x63b6fe2e, 0x923a74ae, 0xc3bee2bd, 0x7e85df58, 0x32ae0249 } } } },
    { 59, 1, "\n", 1, {
        { 6689, IOS_NotonNus, { 0xe8519e6, 0xb7d34e8b, 0xd9eb4e3a, 0xa75d760c, 0x574fea1a } } } },
    { 60, 2, "Used by System Menu 4.0 and 4.1.\n", 2, {
        { 6174, IOS_OK, { 0xf10ee7b, 0xcd26d2b7, 0xf5e9ed58, 0x3f10e561, 0x18be6f84 } },
        { 6400, IOS_Stub, { 0xdb83137b, 0x436b532c, 0x50f147a6, 0xc404dea4, 0x14e87233 } } } },
    { 61, 2, "Used by Shop Channel 4.x & Photo 1.1 Channel v3+\n", 5, {
        { 4890, IOS_OK, { 0x67b07cac, 0x929d12bd, 0xe2e72a0e, 0x73588ede, 0xdd0a2bef } },
        { 5405, IOS_OK, { 0x1eadd8dd, 0xc2d79a8b, 0x2ddcc436, 0xb1fce7c9, 0x576e7ff5 } },
        { 5661, IOS_OK, { 0x4da48c4b, 0x8a165fa5, 0xaceba5c2, 0xe3c93543, 0x3e9b92d7 } },
        { 5662, IOS_OK, { 0xd7b43eff, 0x2b97cc12, 0x92468378, 0xd5a0f618, 0x8c5206a1 } },
        { 5918, IOS_NotonNus, { 0xe19d61b, 0x9669b0cb, 0x9ba654b9, 0xda96f8b8, 0x1ce60efa } } } },
    { 70, 2, "Used by System Menu 4.2.\n", 2, {
        { 6687, IOS_OK, { 0xdea42672, 0x98c32c5c, 0x88da57f7, 0x847a45e9, 0x67b4711c } },
        { 6912, IOS_Stub, { 0x7a09d761, 0x8527768c, 0xc321e460, 0x7f2fc642, 0x28efe171 } } } },
    { 80, 2, "Used by System Menu 4.3.\n", 3, {
        { 6943, IOS_OK, { 0x77a0f0d5, 0xa4af1f45, 0xdf083ce, 0xcb47b57c, 0x34378574 } },
        { 6944, IOS_OK, { 0xeac33c6b, 0x821d111c, 0x4adb9b22, 0x4db436d0, 0xf91ac16f } },
        { 7200, IOS_NotonNus, { 0x644dab0d, 0xa451b1be, 0xedb87440, 0xd991982b, 0x84ec5c48 } } } },
    { 222, 1, "Piracy prevention stub, useless.\n", 1, {
        { 65280, IOS_Stub, { 0x5faf3d89, 0xb2b8ad01, 0x6c629c52, 0x64518df6, 0xc3567d73 } } } },
    { 223, 1, "Piracy prevention stub, useless.\n", 1, {
        { 65280, IOS_Stub, { 0xbcdf9dfd, 0x12a41ae4, 0x1eb74613, 0x69b47652, 0xa6d27966 } } } },
    { 249, 1, "Piracy prevention stub, useless.\n", 1, {
        { 65280, IOS_Stub, { 0xf76e5650, 0xa42b8208, 0xc93f6dd8, 0x22774dc4, 0xa2fe698f } } } },
    { 250, 1, "Piracy prevention stub, useless.\n", 1, {
        { 65280, IOS_Stub, { 0xeb2d3b80, 0x3ca1a09f, 0xdf862b7d, 0xe6768430, 0xde715643 } } } },
    { 254, 1, "Commonly used by BootMii IOS.\nPiracy prevention stub, useless.", 4, {
        { 2, IOS_Stub, { 0x7b61c1f7, 0xd1c4e3fd, 0xd0dfdcb4, 0xfc633e29, 0x6b9ed121 } },
        { 3, IOS_Stub, { 0x2cb06231, 0x9692a096, 0xc6f0db6a, 0xd9636535, 0x75c0131b } },
        { 260, IOS_Stub, { 0xc28d6fde, 0x3371c55d, 0x28714fcf, 0xf97aaaff, 0xf9cc78d0 } },
        { 65280, IOS_Stub, { 0x2c4626ff, 0xce09dd32, 0x64244d7, 0x95e1ac32, 0x72a7ef1e } } } } };
const int GrandIOSLEN = (sizeof(GrandIOSList) / sizeof(IOSlisting));
int LesserIOSLEN;
s32 GrandIOSListLookup[256];
s32 GrandIOSListLookup2[256];

int CheckForIOS(int ver, u16 rev, u16 sysVersion, bool chooseVersion, bool chooseReqIOS) {
    int ret, found2 = GrandIOSListLookup[ver];
    bool patchingSM = chooseVersion || chooseReqIOS;
    bool needsPatch = ( patchingSM && ios_fsign[ver] < 0 && ios_fsign[ver] != -1036);

    if(needsPatch){
        if(ver > 28) {
            u32 curIOS = IOS_GetVersion();
            get_certs();
            ret = ReloadIosNoInitNoPatch(ver);
            if (ret >= 0) {
                checkPatches(ver);
            }
            ReloadIos(curIOS);
            needsPatch = ( patchingSM && ios_fsign[ver] < 0 && ios_fsign[ver] != -1036);
        } else {
            flagOverrideIOSCheck = true;
        }
    }
    if ( !needsPatch && ios_found[ver]) {
        if (ios_found[ver] >= rev) {
            return 0;
        } else if (ios_found[ver] > 0 && found2 >= 0) {
            IOSlisting tempios = GrandIOSList[found2];
            int i = GrandIOSListLookup2[ver];
            if (i >= 0 && tempios.revs[i].num == ios_found[ver]
                    && (tempios.revs[i].mode == IOS_OK || tempios.revs[i].mode
                            == IOS_NotonNus)) {
                return 0;
            }
        }
    }
    printf(
        "You are missing IOS %d, have a stub or too low revision,\n"
        "or are missing Fake Sign patch attempting to install...\n",
        ver);
    printf("\nTry running Check IOSs if you are sure IOS %d is valid", ver);
    PromptAnyKeyContinue();
    patchSettings settings;
    clearPatchSettings(&settings);
    settings.iosVersion = getIOSVerForSystemMenu(sysVersion);
    settings.iosRevision = rev;
    settings.location = ver;
    settings.newrevision = rev;
    settings.esTruchaPatch = patchingSM;
    if( ver <= 30 || found2 < 0 ) {
        settings.iosVersion = 60;
        settings.iosRevision = 6174;
    }
    if( patchingSM ) {
        get_certs();
        int fakesign = check_fakesig();
        if (fakesign == -1036) {
            //This error is ok for what we need to do.
            fakesign = 0;
        }
        settings.haveFakeSign = (fakesign >= 0);
        int nandp = check_nandp();
        settings.haveNandPerm = (nandp >= 0);
    }
    ret = IosInstall(settings);
    if( ret == 1 ) {
        ios_found[ver] = rev;
        ios_fsign[ver] = 0;
    } else {
        ret = -1;
    }
    return ret;
}

int yes_no_checkeach(void) {
    u32 button;
    int mode = 0;
    printf("\t  (A) Yes\t\t(B) No\t(HOME)/GC:(START) Check Each Individually\n");
    flushBuffer();
    while (1) {
        button = WaitButtons();
        if (button & WPAD_BUTTON_A) {
            mode = 2;
            break;
        }
        if (button & WPAD_BUTTON_B) {
            mode = 1;
            break;
        }
        if (button & WPAD_BUTTON_HOME) {
            break;
        }
    }
    return mode;
}

int installIOS(int iosloc) {
    bool exitMenu = false;
    int i, selection = 0;
    u32 button;
    
    IOSlisting tempios = GrandIOSList[iosloc];
    while (!exitMenu) {
        initNextBuffer();
        PrintBanner();
        printf("\n\nSelect which revision of IOS%u to install:\n", tempios.ver);
        for (i = 0; i < tempios.cnt; i += 1) {
            if (i % 11 == 0) {
                printf("\n");
            }
            if (i == selection) {
                set_highlight(true);
                Console_SetFgColor(BLACK, 2);
            }
            if (tempios.revs[i].mode == IOS_Stub) {
                Console_SetFgColor(RED, 1);
            }
            if (tempios.revs[i].mode == IOS_NoNusstub || tempios.revs[i].mode
                    == IOS_NotonNus) {
                Console_SetFgColor(MAGENTA, 0);
            }
            printf(" %5u ", tempios.revs[i].num);
            Console_SetFgColor(WHITE, 0);
            if (i == selection) {
                set_highlight(false);
            }
        }
        printf("\n\nPress D-PAD to select, A to install, B to go back\n");
        printf("IOS revs in ");
        Console_SetFgColor(RED, 1);
        printf("Red");
        Console_SetFgColor(WHITE, 0);
        printf(" are stubs.\nIOS revs in ");
        Console_SetFgColor(MAGENTA, 0);
        printf("Magenta");
        Console_SetFgColor(WHITE, 0);
        printf(" are not available from NUS\n");
        flushBuffer();
        while (1) {
            button = WaitButtons();
            if (button & WPAD_BUTTON_LEFT) {
                selection -= 1;
                if (selection < 0) {
                    selection = tempios.cnt - 1;
                }
                break;
            }
            if (button & WPAD_BUTTON_RIGHT) {
                selection += 1;
                if (selection > (tempios.cnt - 1)) {
                    selection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_UP) {
                selection -= 11;
                if (selection < 0) {
                    selection = tempios.cnt - 1;
                }
                break;
            }
            if (button & WPAD_BUTTON_DOWN) {
                selection += 11;
                if (selection > (tempios.cnt - 1)) {
                    selection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_A) {
                exitMenu = true;
                break;
            }
            if (button & WPAD_BUTTON_B) {
                return -1;
            }
        }
    }
    return selection;
}

int tryInstallIOS(int found2) {
    int selection = installIOS(found2);
    u32 version = IOS_GetVersion();
    if (selection < 0) {
        return false;
    }
    if (GrandIOSList[found2].revs[selection].mode == IOS_Stub
            || GrandIOSList[found2].revs[selection].mode == IOS_NoNusstub) {
        printf("\n\t\t\t\t\t" WARNING_SIGN " WARNING " WARNING_SIGN "\n");
        printf(
            "The revision you have chosen is a stub. A stub can brick your Wii if you are\n");
        printf(
            "unsure of what you are doing. Are you sure you want to install a stub to your Wii?\n");
        if (!PromptYesNo()) {
            return false;
        }
    }
    int tmp = GrandIOSList[found2].ver;

    if (selection >= 0 && doparIos(tmp, GrandIOSList[found2].revs[selection].num, tmp, 0, 0, 0, 1) > 0) {


        if (!wiiSettings.ahbprot || tmp >= 28 ) {
            int ret;
            if (found2 >= 0 && (GrandIOSList[found2].revs[selection].mode
                    == IOS_Stub || GrandIOSList[found2].revs[selection].mode
                    == IOS_NoNusstub)) {
                //printf("Stub %d\n", tmp);
                ret = -1;
            } else {
                ret = ReloadIosNoInit(tmp);
            }
            if (ret >= 0) {
                ios_flash[tmp] = check_flash();
                ios_boot2[tmp] = check_boot2();
                ios_usb2m[tmp] = check_usb2();
                ios_nandp[tmp] = check_nandp();
                ios_fsign[tmp] = check_fakesig();
                ios_sysmen[tmp] = check_sysmenuver();
                ios_ident[tmp] = check_identify();
                ios_found[tmp] = GrandIOSList[found2].revs[selection].num;
            }
            ret = ReloadIos(version);
        }

        updateTitles();
        return true;
    }

    return false;
}

int doparIos(u32 ver, u32 rev, u32 loc, int sigPatch, int esIdentPatch,
    int applynandPatch, int chooseloc) {
    int ret = 0;
    initNextBuffer();
    PrintBanner();

    printf("Are you sure you want to install ");
    Console_SetFgColor(YELLOW, 1);
    printf("IOS%u", ver);
    printf(" v%u", rev);
    Console_SetFgColor(WHITE, 0);
    printf("?\n");
    WiiLightControl(WII_LIGHT_ON);
    flushBuffer();
    if (PromptContinue()) {
        WiiLightControl(WII_LIGHT_OFF);
        patchSettings settings;
        clearPatchSettings(&settings);
        settings.iosVersion = ver;
        settings.iosRevision = rev;
        settings.location = loc;
        ret = configInstallSettings(&settings, sigPatch, esIdentPatch,
            applynandPatch, chooseloc);
        PrintBanner();
        if( ret > 0 ) {
            ret = IosInstall(settings);
        }
        
        if (ret < 0) {
            printf("\n\nERROR: Something failed. (ret: %d)\n", ret);
            PromptAnyKeyContinue();
        }
    }
    WiiLightControl(WII_LIGHT_OFF);
    return ret;
}

u8 choosebaseios(u8 cur) {
    u32 list[] = { 36, 38 };
    int n = 2;
    int i;
    u8 exitMenu = 0, listpick = 0;
    s32 menuSelection = 0, selection = 22, found = GrandIOSListLookup[cur];
    u32 button;
    
    if (found > 0 ) selection = found;

    while (!exitMenu) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n");
        printf("Select base IOS\n");
        for (i = 0; i < n; i += 1) {
            printf("%3s %u\n", (menuSelection == i) ? "-->" : " ", list[i]);
        }
        printf("%3s Any IOS:       \b\b\b\b\b\b", (menuSelection == i)
            ? "-->" : " ");
        set_highlight(true);
        printf("IOS%u\n", GrandIOSList[selection].ver);
        set_highlight(false);
        printf("Press B to go back\n");
        flushBuffer();
        while (1) {
            button = WaitButtons();
            if (button & WPAD_BUTTON_DOWN) {
                menuSelection += 1;
                if (menuSelection > n) {
                    menuSelection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_UP) {
                menuSelection -= 1;
                if (menuSelection < 0) {
                    menuSelection = n;
                }
                break;
            }
            if (button & WPAD_BUTTON_LEFT) {
                if (menuSelection == i) {
                    selection -= 1;
                    if (selection < 0) {
                        selection = GrandIOSLEN - 1;
                    }
                    while (!ios_found[GrandIOSList[selection].ver]) {
                        selection -= 1;
                        if (selection < 0 || GrandIOSList[selection].ver == 0) {
                            selection = GrandIOSLEN - 1;
                            continue;
                        }
                    }
                    break;
                }
            }
            if (button & WPAD_BUTTON_RIGHT) {
                if (menuSelection == i) {
                    selection += 1;
                    if (selection > GrandIOSLEN - 1) {
                        selection = 1;
                    }
                    while (!ios_found[GrandIOSList[selection].ver]) {
                        selection += 1;
                        if (selection > GrandIOSLEN - 1
                                || GrandIOSList[selection].ver == 0) {
                            selection = 1;
                            continue;
                        }
                    }
                    break;
                }
            }
            if (button & WPAD_BUTTON_A) {
                if (menuSelection < n) {
                    listpick = 1;
                }
                exitMenu = true;
                break;
            }
            if (button & WPAD_BUTTON_B) {
                return 0;
            }
        }
    }
    if (listpick) {
        selection = found;
        s32 temp = list[menuSelection];
        if (temp > 2 && temp < 256 && GrandIOSListLookup[temp] >= 0) {
            selection = GrandIOSListLookup[temp];
        }
    }
    return selection;
}

u8 chooseiosloc(char *str, u8 cur) {
    u32 list[] = { 202, 222, 223, 236, 249, 250 };
    int n = 6;
    int i;
    u8 exitMenu = 0, listpick = 0;
    s32 menuSelection = 0;
    u8 selection = cur;
    u32 button;
    while (!exitMenu) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n");
        printf("%s",str);
        for (i = 0; i < n; i += 1) {
            printf("%3s %u\n", (menuSelection == i) ? "-->" : " ", list[i]);
        }
        printf("%3s Any IOS:       \b\b\b\b\b\b", (menuSelection == i)
            ? "-->" : " ");
        set_highlight(true);
        printf("IOS%u\n", selection);
        set_highlight(false);
        printf("Press B to go back\n");
        flushBuffer();
        while (1) {
            button = WaitButtons();
            if (button & WPAD_BUTTON_DOWN) {
                menuSelection += 1;
                if (menuSelection > n) {
                    menuSelection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_UP) {
                menuSelection -= 1;
                if (menuSelection < 0) {
                    menuSelection = n;
                }
                break;
            }
            if (button & WPAD_BUTTON_LEFT) {
                if (menuSelection == i) {
                    selection -= 1;
                }
                if (selection < 3) {
                    selection = 255;
                }
                break;
            }
            if (button & WPAD_BUTTON_RIGHT) {
                if (menuSelection == i) {
                    selection += 1;
                }
                if (selection == 0) {
                    selection = 3;
                }
                break;
            }
            if (button & WPAD_BUTTON_A) {
                if (menuSelection < n) {
                    listpick = 1;
                }
                exitMenu = true;
                break;
            }
            if (button & WPAD_BUTTON_B) {
                return 0;
            }
        }
    }
    if (listpick) {
        selection = list[menuSelection];
    }
    return selection;
}

int DeleteIOS( u32 loc ) {
    PrintBanner();
    int ret = 0;
    printf("\nAre you sure you want to delete IOS %d?\n\n", loc);
    if (PromptContinue()) {
        u64 titleId;
        u32 version = 0;
        version = IOS_GetVersion();
        ret = 1;

        // HBC v1.0.7+
        int newhbcios = TITLE_LOWER(theSettings.hbcRequiredTID[2]);
        // HBC JODI
        int otherhbccios = TITLE_LOWER(theSettings.hbcRequiredTID[1]);
        // HBC HAXX
        int oldhbcios = TITLE_LOWER(theSettings.hbcRequiredTID[0]);
        // HBC MAUI (Backup HBC)
        int mauihbcBackup = TITLE_LOWER(theSettings.hbcRequiredTID[3]);
        if (loc == newhbcios || loc == otherhbccios || loc
                == oldhbcios || loc == mauihbcBackup ) {
            PrintBanner();
            printf("\n\t\t\t\t\t" WARNING_SIGN " WARNING " WARNING_SIGN "\n");
            printf(
                "BRICK ALERT\n\nThe IOS you have selected %d is used by\n",
                loc);
            printf(
                "Homebrew Channel, it is possible to brick your Wii by deleting this.\n");
            printf("Are you really sure you want to delete this?\n\n");
            ret = PromptContinue();
        }
        if (wiiSettings.sysMenuIOS == loc) {
            PrintBanner();
            printf("\n\t\t\t\t\t" WARNING_SIGN " WARNING " WARNING_SIGN "\n");
            printf(
                "BRICK ALERT\n\nThe IOS you have selected is used by your System Menu,\n");
            printf(
                "it is possible to brick your Wii by deleting this.\n");
            printf("Are you really sure you want to delete this?\n\n");
            ret = PromptContinue();
        }
        if (ios_used[loc] > 0) {
            PrintBanner();
            printf(
                "You may not want to delete this IOS. It is currently being used by :: \n");
            u32 idx;
            for (idx = 0; idx < ios_used[loc] && idx < 20; idx += 1) {
                printf("\t\t\t%s\n", namesTitleUsingIOS[loc][idx]);
            }
            ret = PromptContinue();
        }
        if (ret) {
            get_certs();
            int nandcheck = check_nandp();
            ios_nandp[version] = nandcheck;
            while (check_sysmenuver() < 0 || version == loc
                    || nandcheck < 0) {
                if (ret != 0) {
                    version = ret;
                    ret = ReloadIos(ret);
                    nandcheck = check_nandp();
                    if (nandcheck < 0) {
                        printf(
                            "\nNand Permissions required, check your IOSs for a valid IOS\n");
                        PromptAnyKeyContinue();
                    } else if (version == loc) {
                        printf(
                            "\nYou can't delete IOS %d with IOS %d\n",
                            version, version);
                        PromptAnyKeyContinue();
                    } else if (check_sysmenuver() > 0) {
                        break;
                    }
                }
                ret = ios_selectionmenu(36);
                if (ret == 0) {
                    ret = -1;
                    break;
                }
            }
            if (ret >= 0) {
                titleId = TITLE_ID(1, loc);

                if (ISFS_Initialize() == 0) {
                    Uninstall_FromTitle(titleId);
                    ISFS_Deinitialize();
                    PromptAnyKeyContinue();
                    updateTitles();
                    ClearPermDataIOS(loc);
                }
            }
        }
    }
    return ret;
}


int installCios(u32 curSelIOS) {
    int found = ios_found[curSelIOS];
    u8 baseios = (found) ? choosebaseios(curSelIOS) : choosebaseios(36);
    if (baseios == 0) {
        return 0;
    }
    int selection = installIOS(baseios);
    if (GrandIOSList[baseios].revs[selection].mode == IOS_Stub
            || GrandIOSList[baseios].revs[selection].mode
                    == IOS_NoNusstub) {
        printf("\n\t\t\t\t\t" WARNING_SIGN " WARNING " WARNING_SIGN "\n");
        printf("The revision you have chosen is a stub.\n");
        printf(
            "A stub can brick your Wii if you are unsure what you are doing.\n");
        printf("Are you sure you want to install a stub to your Wii?\n");
        if (!PromptYesNo()) {
            return 0;
        }
    }
    if (selection < 0) {
        return 0;
    }

    u8 loc = (found) ? chooseiosloc("Select location of IOS\n", curSelIOS) : chooseiosloc("Select location of IOS\n", 36);
    int ret = 1;
    if (loc == wiiSettings.sysMenuIOS) {
        PrintBanner();
        printf(
            "BRICK ALERT\n\nThe IOS you have selected is used by your System Menu,\n");
        printf(
            "it is possible to brick your Wii by overwriting this.\n");
        printf("Are you really sure you want to overwrite this?\n\n");
        ret = PromptContinue();
    }
    if (baseios >= 0 && loc > 2 && ret) {
        u32 version = IOS_GetVersion();
        if (selection >= 0 && doparIos(GrandIOSList[baseios].ver,
            GrandIOSList[baseios].revs[selection].num, loc, 0, 0, 0, 1) > 0) {
            if (!wiiSettings.ahbprot || GrandIOSList[baseios].ver >= 28 ) {
                int tmp = loc;
                if (found >= 0 && (GrandIOSList[baseios].revs[selection].mode == IOS_Stub
                        || GrandIOSList[baseios].revs[selection].mode == IOS_NoNusstub)) {
                    printf("Stub %d\n", tmp);
                    ret = -1;
                } else {
                    ret = ReloadIosNoInit(tmp);
                }
                if (ret >= 0) {
                    ios_flash[tmp] = check_flash();
                    ios_boot2[tmp] = check_boot2();
                    ios_usb2m[tmp] = check_usb2();
                    ios_nandp[tmp] = check_nandp();
                    ios_fsign[tmp] = check_fakesig();
                    ios_sysmen[tmp] = check_sysmenuver();
                    ios_ident[tmp] = check_identify();
                    ios_found[tmp] = GrandIOSList[baseios].revs[selection].num;
                    ret = ReloadIos(version);
                    ret = 1;
                }
            }
            printf("\nFinished installing cIOS to %u.\n", loc);
            PromptAnyKeyContinue();
            updateTitles();
        }
    }
    return ret;
}

int UpdateAllIOSs() {
    PrintBanner();
    printf("\nAre you sure you want to update all available IOSs?\n\n");
    flushBuffer();
    if (PromptContinue()) {
        int i, sighash, esident, nandpatch, chooseloc;
        printf(
            "\nApply Sig Hash Check patch to all IOSs that it can?\n");
        sighash = yes_no_checkeach();
        printf("\nApply ES_Identify patch to all IOSs that it can?\n");
        esident = yes_no_checkeach();
        printf("\nApply Nand patch to all IOSs that it can?\n");
        nandpatch = yes_no_checkeach();
        printf(
            "\nDo you wish to choose the install location of all IOSs?\n");
        chooseloc = PromptYesNo();
        PrintBanner();

        for (i = 0; i < GrandIOSLEN; i += 1) {
            if (GrandIOSList[i].onnus == 2) {
                printf(".");
                int j;
                for (j = GrandIOSList[i].cnt - 1; j >= 0; j -= 1) {
                    if (GrandIOSList[i].revs[j].mode == IOS_OK) {
                        break;
                    }
                }
                u16 curRev = ios_found[GrandIOSList[i].ver];
                if (curRev > 65535) {
                    curRev = 0;
                }
                if (GrandIOSList[i].revs[j].num > curRev) {
                    doparIos(GrandIOSList[i].ver,
                        GrandIOSList[i].revs[j].num,
                        GrandIOSList[i].ver, sighash, esident,
                        nandpatch, chooseloc);
                }
            }
        }
        updateTitles();
        checkIOSConditionally();
        WPAD_Init();
        WiiLightControl(WII_LIGHT_ON);
        usleep(500000);
        WiiLightControl(WII_LIGHT_OFF);
        usleep(500000);
        WiiLightControl(WII_LIGHT_ON);
        usleep(500000);
        WiiLightControl(WII_LIGHT_OFF);
        printf("Finished updating all IOSs\n");
        PromptAnyKeyContinue();
    }
    return 1;
}

int CopyChangeRev( u32 loc ) {
    int ret = 0;
    patchSettings settings;
    clearPatchSettings(&settings);
    settings.iosVersion = loc;
    u8 retloc = (ios_found[loc]) ? chooseiosloc("Select location of IOS\n", loc) : chooseiosloc("Select location of IOS\n", 36);
    if( retloc > 0 ) {
        settings.location = retloc;
        ret = configInstallSettings(&settings, 0, 0, 0, 1);
    } else {
        ret = -1;
    }
    if( ret > 0 && Nand_Init() >= 0 ) {
        ret = IOSMove(settings);
        ISFS_Deinitialize();
        PromptAnyKeyContinue();
        updateTitles();
    }
    return 0;
}

void DisplayIOSVersion( u32 loc, s32 found, s32 found2, s32 latestNonStub ) {
    u32 printed = 0;
    u16 installed = ios_found[loc];
    char str[7];
    snprintf(str, sizeof(str), "IOS%u", loc);
    printf("\n\nCurrently selected: %6s\t\tVersion installed:\t", str);
    if (found >= 0 && ios_isStub[loc] > 1) {
        printf("(stub)");
        printed = 1;
    }
    if (!printed && installed) {
        printf("v%d", installed);
        if (ios_isStub[loc] > 0) {
            printf("(maybe_stub)");
        }
    } else if (!printed) {
        printf("(none)");
    }
    if (found >= 0) {
        IOSlisting tempios = GrandIOSList[found];
        snprintf(str, sizeof(str), "v%u", tempios.revs[tempios.cnt - 1].num);
        printf("\n\t\tLatest: %6s", str);
        printf("\t\t\tLatest non-stub:\t");
        if (found2 > 0) {
            printf("v%u", tempios.revs[latestNonStub].num);
            if (tempios.revs[latestNonStub].mode == IOS_NotonNus) {
                printf("(Not on NUS)");
            }
        } else {
            printf("(none)");
        }
    } else {
        printf("\n");
    }
    if (ios_clean[loc] == 2) {
        Console_SetFgColor(MAGENTA, 0);
        printf("\nYou have a special ios, post your csv online to see if you're a winner.\n");
        Console_SetFgColor(WHITE, 0);
    } else {
        printf("\n\n");
    }
}

void DisplayIOSPerms( u32 loc, s32 found ) {
    u16 installed = ios_found[loc];
    if (found >= 0) {
        Console_SetFgColor(WHITE, BOLD_NORMAL);
        printf("Description\n");
        Console_SetFgColor(CYAN, 0);
        printf("%s", GrandIOSList[found].Description);
        Console_SetFgColor(WHITE, 0);
    } else {
        printf("\n\n");
    }
    if (installed && theSettings.printiosdetails) {
        printf("\nFakesign Bug (Trucha bug):");
        int fsig = ios_fsign[loc];
        Console_SetFgColor((fsig >= 0) ? YELLOW : RED, 1);
        printf("%s", (fsig >= 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);

        printf("\t\t EsIdentify (ES_DiVerify):");
        int esident = ios_ident[loc];
        Console_SetFgColor((esident >= 0) ? YELLOW : RED, 1);
        printf("%s\n", (esident >= 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);

        printf("\t/dev/flash (Flash access):");
        int devflash = ios_flash[loc];
        Console_SetFgColor((devflash >= 0) ? YELLOW : RED, 1);
        printf("%s", (devflash >= 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);

        printf("\tUSB2 Tree:");
        int devusb = ios_usb2m[loc];
        Console_SetFgColor((devusb >= 0) ? YELLOW : RED, 1);
        printf("%s", (devusb >= 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);

        printf("\tboot2:");
        int devboot2 = ios_boot2[loc];
        Console_SetFgColor((devboot2 >= 0) ? YELLOW : RED, 1);
        printf("%s\n", (devboot2 >= 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);

        printf("\tNAND Permissions:");
        int nandp = ios_nandp[loc];
        Console_SetFgColor((nandp >= 0) ? YELLOW : RED, 1);
        printf("%s", (nandp >= 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);

        printf("\t\tGetSysMenuVersion:");
        u16 sysmen = ios_sysmen[loc];
        Console_SetFgColor((sysmen > 0) ? YELLOW : RED, 1);
        printf("%s\n", (sysmen > 0) ? "[Yes]" : "[No]");
        Console_SetFgColor(WHITE, 0);
    } else {
        printf("\n\n");
    }
}

int ManageIOS( u32 loc ) {
    static int menuItem = 0;
    u32 button = 0;
    s32 found = 0, ret = 0, ret2 = 0, j, found2, latestNonStub=0, init=1;

    while (!(button & WPAD_BUTTON_B)) {
        if( init ) {
            found = GrandIOSListLookup[loc];

            if (found >= 0) {
                IOSlisting tempios = GrandIOSList[found];
                for (j = 0; j < tempios.cnt; j += 1) {
                    if (tempios.revs[j].mode == IOS_OK || tempios.revs[j].mode
                            == IOS_NotonNus) {
                        latestNonStub = j;
                        found2 = 1;
                    }
                }
            } else {
                menuItem = 1;
            }
            init =0;
        }

        initNextBuffer();
        PrintBanner();
        DisplayIOSVersion( loc, found, found2, latestNonStub );
        if(found >= 0) {
            printf("\n\t\t\t%3s Install IOS\n", (menuItem == 0) ? "-->" : " ");
        }
        printf("\t\t\t%3s Extract to Wad\n", (menuItem == 1) ? "-->" : " ");
        printf("\t\t\t%3s Delete IOS\n", (menuItem == 2) ? "-->" : " ");
        printf("\t\t\t%3s Copy / Change Version / Patch IOS\n\n\n\n\n", (menuItem == 3) ? "-->" : " ");
        DisplayIOSPerms( loc, found );
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW,
            DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return -1;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_A) {
            if (found >= 0 && menuItem == 0) {
                ret2 = tryInstallIOS(found);
                init = 1;
            } else if (menuItem == 1) {
                ret2 = DumpIOStoSD(loc);
            } else if (menuItem == 2) {
                ret2 = DeleteIOS(loc);
                if(ret2 > 0) {
                    ret = ret2;
                }
                if(found < 0){
                    break;
                }
                init = 1;
            } else if (menuItem == 3) {
                ret2 = CopyChangeRev(loc);
                init = 1;
            }
            if( ret2 > 0 ) {
                ret = ret2;
            }
        }
        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;

        if (menuItem > 3) {
            menuItem = (found >= 0)?0:1;
        }
        if (menuItem < ((found >= 0)?0:1)) menuItem = 3;
    }
    return ret;
}

int removeStubsMenu() {
    int ret = check_nandp();
    if (ret < 0) {
        initNextBuffer();
        PrintBanner();
        printf(
            "\n\nNand Permissions required, check your IOSs for a valid IOS\n");
        flushBuffer();
        PromptAnyKeyContinue();
    } else {
        initNextBuffer();
        PrintBanner();
        printf(
            "Are you sure you want to check for stub IOSs and delete them?\n");
        flushBuffer();
        if (PromptContinue() && !CheckAndRemoveStubs()) {
            printf("\n\nNo stubs found!");
            PromptAnyKeyContinue();
        }
        updateTitles();
        ret = 1;
    }
    return ret;
}

int IOSMenu() {
    static int selected = 36, listlen = 0;
    static int list2[256];
    
    if (wiiSettings.sysMenuVer < 0) {
        initNextBuffer();
        PrintBanner();
        printf("\n\nThe IOS you have loaded can't get the IOS version list\n");
        printf("Try to reload an IOS > 22\n");
        flushBuffer();
        PromptAnyKeyContinue();
        return 0;
    }

    if (theSettings.neverSet == 0) {
        listlen = getsuplist(list2);
        theSettings.neverSet = 1;
    }

    u8 temp;
    int found, found2, i;
    u32 button = 0, j, latestNonStub;
    
    while (!(button & WPAD_BUTTON_B)) {
        found = -1;
        found2 = -1;
        latestNonStub = 0;
        temp = list2[selected];
        initNextBuffer();
        PrintBanner();
        printf("Select the IOS to manage.\t\t\t\t(IOSs in ");
        Console_SetFgColor(GREEN, 0);
        printf("Green");
        Console_SetFgColor(WHITE, 0);
        printf(" are installed on this Wii)\n");
        printf("\t\t\t\t\t\t\t\t\t(IOSs in ");
        Console_SetFgColor(CYAN, 0);
        printf("Cyan");
        Console_SetFgColor(WHITE, 0);
        printf(" are clean)\n");
        if (checkMissingSystemMenuIOS(0)) {
            printf(
                "\t\t\t"WARNING_SIGN" BRICK ALERT - Missing System Menu IOS - BRICK ALERT "WARNING_SIGN"\n");
            Console_SetFgColor(WHITE, 0);
        } else {
            printf("\n");
        }

        found = GrandIOSListLookup[temp];
        for (i = 0; i < listlen; i += 1) {
            u8 colorCode = WHITE, colorCode2 = BLACK;

            if (wiiSettings.sysMenuIOS == list2[i]
                    && (checkMissingSystemMenuIOS(0)
                            || checkSystemMenuIOSisNotStub(0))) {
                colorCode = RED;
            } else if (ios_clean[list2[i]] == 2) {
                colorCode = MAGENTA;
            } else if (ios_isStub[list2[i]] > 1) {
                colorCode = YELLOW;
            } else if (ios_clean[list2[i]] == 1) {
                colorCode = CYAN;
            } else if (ios_found[list2[i]]) {
                colorCode = GREEN;
            } else if (ios_used[list2[i]] > 0) {
                colorCode = YELLOW;
            }
            if (i == selected) {
                if (colorCode == WHITE) colorCode = BLACK;
                colorCode2 = WHITE;
            }
            Console_SetFgColor(colorCode, 0);
            Console_SetBgColor(colorCode2, 0);
            printf(" %3u ", list2[i]);
            Console_SetFgColor(WHITE, 0);
            Console_SetBgColor(BLACK, 0);
        }
        if (found >= 0) {
            IOSlisting tempios = GrandIOSList[found];
            for (j = 0; j < tempios.cnt; j += 1) {
                if (tempios.revs[j].mode == IOS_OK || tempios.revs[j].mode
                        == IOS_NotonNus) {
                    latestNonStub = j;
                    found2 = 1;
                }
            }
        }
        DisplayIOSVersion( temp, found, found2, latestNonStub );
        DisplayIOSPerms( temp, found );
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tSelect IOS\t\t\t\t(-)/GC:(L)\tRemove Stubs\n", UP_ARROW, DOWN_ARROW,
            LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tManage IOS %u\t\t\t\t", temp);
        printf("(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit\t");
        if (checkMissingSystemMenuIOS(0)) {
            Console_SetFgColor(RED, 0);
        } else if (wiiSettings.missingIOSwarning) {
            Console_SetFgColor(YELLOW, 0);
        }
        printf("(1)/(Y) Update All IOSs\t");
        Console_SetFgColor(WHITE, 0);
        printf("(2)/(X) Install cIOS");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return 0;

        if (button & WPAD_BUTTON_RIGHT) {
            selected++;
            if (selected > listlen - 1) {
                selected = 0;
            }
        }
        if (button & WPAD_BUTTON_LEFT) {
            selected--;
            if (selected < 0) {
                selected = listlen - 1;
            }
        }
        if (button & WPAD_BUTTON_DOWN) {
            selected += 16;
            if (selected > listlen - 1) {
                selected = 0;
            }
        }

        if (button & WPAD_BUTTON_UP) {
            selected -= 16;
            if (selected < 0) {
                selected = listlen - 1;
            }
        }
        if (button & WPAD_BUTTON_A) {
            if (ManageIOS(temp)) {
                listlen = getsuplist(list2);
            }
        }
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_1) {
            if( UpdateAllIOSs() > 0 ) {
                listlen = getsuplist(list2);
            }
        }
        if (button & WPAD_BUTTON_2) {
            if( installCios(temp) > 0 ) {
                listlen = getsuplist(list2);
            }
        }
        if (button & WPAD_BUTTON_MINUS) {
            if( removeStubsMenu() > 0 ) {
                listlen = getsuplist(list2);
            }
        }
    }
    return 0;
}
