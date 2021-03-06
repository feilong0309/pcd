

from nose.tools import *

def test_sqlitedict():
    import pcd.sqlite

    d = pcd.sqlite.SQLiteDict(':memory:')
    d0 = {1: 15, 2: 'b', (3,'c'): 4}

    assert_equal(len(d), 0)

    for k, v in d0.items():
        d[k] = v

    assert_equal(len(d), 3)

    assert_dict_equal(d0, dict(d0.iteritems()))

    # __getitem__ and __setitem__
    for k in d0:
        assert_equal(d0[k], d[k])

    # get
    assert_equal(d.get(1), 15)
    assert_equal(d.get(-100, 'default'), 'default')

    # Check basic keys, values, items
    assert_equal(set(d0.keys()),   set(d.keys()))
    assert_equal(set(d0.values()), set(d.values()))
    assert_equal(set(d0.items()),  set(d.items()))

    assert_equal(set(d0.iterkeys()),   set(d.iterkeys()))
    assert_equal(set(d0.itervalues()), set(d.itervalues()))
    assert_equal(set(d0.iteritems()),  set(d.iteritems()))

    # __iter__
    assert_equal(set(d0.iterkeys()),   set(d))

    # update
    d = pcd.sqlite.SQLiteDict(':memory:')
    d.update(d0)
    assert_dict_equal(d0, dict(d0.iteritems()))

    # update
    d = pcd.sqlite.SQLiteDict(':memory:')
    d.update(list(d0.iteritems()))
    assert_dict_equal(d0, dict(d0.iteritems()))

def test_sqlite_corruption():
    import pcd.sqlite
    import cPickle as pickle

    d = pcd.sqlite.SQLiteDict(':memory:')
    d0 = {1: 15, 2: 'b', (3,'c'): 4}
    for k, v in d0.items():
        d[k] = v

    assert_false(d._find_corruption())
    assert_equal(len(d), 3)

    # Add a hash mismatch
    c = d.conn.cursor()
    c.execute('insert into dict values (?,?,?)',
              (1313,
               buffer(pickle.dumps('corrupted',  -1)),
               buffer(pickle.dumps('corrupted_value',  -1))))
    d.conn.commit()
    c.close()

    assert_equal(len(d), 4)
    assert_true(d._find_corruption())
    assert_true(d._find_corruption()) # it doesn't fix it on prev call
    assert_true(d._find_corruption(remove=True))
    assert_false(d._find_corruption())
    assert_false(d._find_corruption(remove=True))


    assert_equal(set(d0.iterkeys()),   set(d.iterkeys()))
    assert_equal(set(d0.itervalues()), set(d.itervalues()))
    assert_equal(set(d0.iteritems()),  set(d.iteritems()))
