#include "isotoxin.h"
#include "vector"

#ifndef _FINAL

#if 0
struct txxx
{
    int a;
    txxx(int a) :a(a) {}
    ~txxx()
    {
        WARNING("%i", a);
    }
};

struct tests
{
    MOVABLE(false);
    DUMMY(tests);
    txxx *d;
    tests() :d(0) {}
    ~tests()
    {
        if (d) TSDEL(d); else WARNING("NULL");
    }

    tests(const tests & o)
    {
        d = nullptr;
    }
    //tests( tests && o)
    //{
    //    d = o.d;
    //    o.d = nullptr;
    //}

};
#endif

ts::random_modnar_c r1, r2;

template< size_t B, size_t CNT, typename RVT > void t( ts::packed_buf_c<B,CNT,RVT> & b )
{
    uint seed = r1.get_next();
    r2.init(seed);

    for(int i=0;i<CNT;++i)
    {
        RVT v = (RVT)r2.get_next( 1<<B );
        b.set( i, v );
    }

    r2.init(seed);

    for (int i = 0; i < CNT; ++i)
    {
        RVT v = (RVT)r2.get_next(1<<B);
        RVT vc = b.get(i);
        ASSERT(v == vc, "ts::packed_buf_c");
    }
}

void crash()
{
    //*((int *)1) = 1;
}

void dotests()
{
    //uint64 u1 = ts::uuid();
    //uint64 u2 = ts::uuid();
    //uint64 u3 = ts::uuid();

    // test packed_buf_c

    //ts::packed_buf_c<20, 121, int> b0;

    ts::packed_buf_c<1,121> b1;
    ts::packed_buf_c<2,113> b2;
    ts::packed_buf_c<3,97> b3;
    ts::packed_buf_c<4,3> b4;
    ts::packed_buf_c<5,3> b5;
    ts::packed_buf_c<6,3> b6;
    ts::packed_buf_c<7,72> b7;
    ts::packed_buf_c<8,11> b8;
    ts::packed_buf_c<9,1311> b9;
    ts::packed_buf_c<1,1> b10;
    ts::packed_buf_c<9,2> b11;
    ts::packed_buf_c<10,2> b12;
    t(b1);
    t(b2);
    t(b3);
    t(b4);
    t(b5);
    t(b6);
    t(b7);
    t(b8);
    t(b9);
    t(b10);
    t(b11);
    t(b12);

    //ARRAY_TYPE_INPLACE( draw_data_s, 4 ) ta;
    //ta.duplast();
    //ta.duplast();
    //ta.duplast();
    //ta.duplast();
    //ta.duplast();
    //ta.truncate( ta.size() - 1 );
    //ta.truncate( ta.size() - 1 );
    //ta.truncate( ta.size() - 1 );
    //ta.truncate( ta.size() - 1 );

    //rectengine_c h1, h2;
    //ARRAY_TYPE_SAFE(sqhandler_i,1) iii;
    //iii.set(2, &h1);
    //iii.set(7, &h2);

    //int sum = 0;
    //for( sqhandler_i *el : iii )
    //{
    //    sum += (int)el;
    //}

   // __debugbreak();

#if 0
    {
        ts::uint8 bbb[sizeof(tests)];
        tests *jjj = (tests *)&bbb;
        tests kkk; kkk.d = TSNEW(txxx, 111);
        TSPLACENEW(jjj,kkk);



        ARRAY_TYPE_INPLACE(tests, 4) tar;
        tar.add().d = TSNEW(txxx, 11);
        tar.add().d = TSNEW(txxx, 12);
        tar.add().d = TSNEW(txxx, 13);
        tar.add().d = TSNEW(txxx, 14);
        tar.add().d = TSNEW(txxx, 15);
        tests tt = tar.get_remove_slow(0);
        tar.add().d = TSNEW(txxx, 16);


    }

    __debugbreak();
#endif

	/*
	//m_cmdpars.qsplit( cmdl );
	//__int64 ggg = ts::zstrings_internal::maximum<uint>::value;

	const ts::wchar *ttt = L"-1123.171a";
	double xxx;
	bool ok = ts::CHARz_to_double(xxx,ttt,7);

    ts::vec2 v(1.3,2.7);
    ts::ivec3 v3(3,7,11);

    ts::aligned<double> ad(53.0);

	ts::wstr_c s(L"-55,12");
	ts::wstr_c x(s);
	ts::str_c z("ansi");
    ts::str_c zzz;

	int r = sizeof(z);

	//s.insert(0,L"nnn");
	int t1;
	double t2;
	//ts::wstr_c t1, t2;
	s.split(t1,t2,",");
	s.set_as_utf8("abc");
	//z = s;

	ts::swstr_c s1(z);
	z += s;
	ts::swstr_t<16> s2(s1);
	ts::sstr_t<32> s3(s2);
	wchar_t *yyy=(wchar_t *)&s2;

    ts::str_t<char, ts::str_core_static_c<char, 32> > hhh("nnff");

	s1 += s2;
	s1 += z;


	z = s1 + s2;
	s1 = z + s2;

	//s1 = z;
	s2 = x;

    ts::strings_c<wchar_t> sa1;
    sa1.add(L"s1");
    sa1.add(L"s2");
    sa1.add(L"s3");

	ts::type_buf_c<int> bbb;
	bbb.add(12);
	bbb.add(13);

	ts::hashmap_t<int, int> hm;
	hm[1] = 2;
	hm[3] = 4;

	__debugbreak();
	*/

    crash();

}




class test_window : public gui_isodialog_c
{
    gui_message_item_c *ll;
    ts::shared_ptr<contact_c> c;
protected:
    // /*virtual*/ int unique_tag() { return UD_NOT_UNIQUE; }
    /*virtual*/ void created() override
    {
        set_theme_rect(CONSTASTR("main"), false);
        __super::created();

        c = TSNEW( contact_c );
        c->set_name(L"Bla");

        gui_message_item_c &l = MAKE_CHILD<gui_message_item_c>(getrid(), c, CONSTASTR("mine"), MTA_MESSAGE);

        post_s p;
        //p.time = now() - 1000;
        //p.message = L"А теперь представьте, что к вам домой приезжает специальный человек, забирает всеваше ненужное барахло и дает вам деньги. Вам не надо выставлять на продажу ваши вещи, вам не надо их фотографировать и встречаться с покупателями. За вас все сделают. Ненужные вещи есть у каждого человека";
        ////p.message = L"унделиверед";
        //l.append_text(p);

        p.time = now() - 500;
        p.message = L"ПреведмедведычПреведмедведыч(facepalm)Преведмедве(colb)дычПреведмедведычПреведмедведычПреведмедведыч";
        l.append_text(p);

        p.time = now() - 2100;
        //p.type = MTA_UNDELIVERED_MESSAGE;
        p.message = L"унделиверед";
        //l.leech( TSNEW(leech_fill_parent_rect_s, ts::irect(0,0,0,40)) );
        //l.set_text(L"А теперь представьте, что к вам домой приезжает <p><color=#FF0000>специальный человек, забирает все</color><p>ваше ненужное барахло и дает вам деньги. Вам не надо выставлять на продажу ваши вещи, вам не надо их фотографировать и встречаться с покупателями. За вас все сделают. Ненужные вещи есть у каждого человека. 
        //p.message = L"Круг потенциальных клиентов огромен.Это каждый из вас.Уверен, что за 10 минут вы соберете целую коробку ненужного вам барахла : старая техника, сломанный телефон или телевизор, бабушкин шкаф, старая одежда и книги.Возможно кто - то подарил вам что - то ненужное и вы бы хотели получить за это деньги ? Кто - то переезжает и ему нужно куда - то деть старые вещи ? Вы закрываете офис и у вас остается куча техники и мебели ? Вот тут и нужен Макс.Он приедет и заберет все.Вы сразу получите деньги.";
        //l.set_selectable();
        l.append_text(p);


        ll = &l;
    }

public:
    test_window(initial_rect_data_s &data):gui_isodialog_c(data) {}
    ~test_window() {}

    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(150, 150); }

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
    {
        if (qp == SQ_PARENT_RECT_CHANGED)
        {
            ts::irect cr = get_client_area();

            int h = ll->get_height_by_width(cr.width());
            if (h > cr.height() - 40) h = cr.height() -40;

            MODIFY(*ll).pos(cr.lt).size(cr.width(), h);
            return false;
        }
    
        return __super::sq_evt(qp,rid,data);
    }

    /*virtual*/ void on_confirm() override
    {
        ll->dirty_height_cache();
        ts::irect cr = get_client_area();
        int h = ll->get_height_by_width(cr.width());
        DMSG("h:" << h << getprops().size());

        ll->getengine().redraw();
    }
};





bool zero_version = false;


void summon_test_window()
{
    //cfg().autoupdate_next( now() + 2 );
    //cfg().autoupdate_newver(CONSTASTR("0.0.0"));
    //zero_version = true;
    //gmsg<ISOGM_NEWVERSION> *m = TSNEW(gmsg<ISOGM_NEWVERSION>, CONSTASTR("0.1.225"));
    //m->send_to_main_thread();


    
    drawchecker dch;
    RID r = MAKE_ROOT<test_window>(dch);
    MODIFY(r).allow_move_resize().size(502,447).setcenterpos().visible(true);
    
}







#endif
