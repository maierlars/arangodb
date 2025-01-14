/* jshint globalstrict:false, strict:false */
/* global assertEqual, assertNotEqual, assertFalse,
   print, print_plain, COMPARE_STRING, NORMALIZE_STRING,
   help, start_pager, stop_pager, start_pretty_print, stop_pretty_print, start_color_print, stop_color_print */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for client-specific functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var db = require('@arangodb').db;

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function clientTestSuite () {
  'use strict';
  return {
    
    testIsArangod: function () {
      assertFalse(require("internal").isArangod());
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test global help function
// //////////////////////////////////////////////////////////////////////////////

    testHelp: function () {
      help();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test start_pager function
// //////////////////////////////////////////////////////////////////////////////

    testPagerStart: function () {
      start_pager();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test stop_pager function
// //////////////////////////////////////////////////////////////////////////////

    testPagerStop: function () {
      stop_pager();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test start_pager function
// //////////////////////////////////////////////////////////////////////////////

    testPagerStartStart: function () {
      start_pager();
      start_pager();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test stop_pager function
// //////////////////////////////////////////////////////////////////////////////

    testPagerStopStop: function () {
      stop_pager();
      stop_pager();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test start_pretty_print function
// //////////////////////////////////////////////////////////////////////////////

    testPrettyStart: function () {
      start_pretty_print();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test stop_pretty_print function
// //////////////////////////////////////////////////////////////////////////////

    testPrettyStop: function () {
      stop_pretty_print();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test start_color_print function
// //////////////////////////////////////////////////////////////////////////////

    testColorStart: function () {
      start_color_print();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test stop_color_print function
// //////////////////////////////////////////////////////////////////////////////

    testColorStop: function () {
      stop_color_print();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test print function
// //////////////////////////////////////////////////////////////////////////////

    testPrint: function () {
      print(true);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test print_plain function
// //////////////////////////////////////////////////////////////////////////////

    testPrintPlain: function () {
      print_plain(true);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test COMPARE_STRING function
// //////////////////////////////////////////////////////////////////////////////

    testICU_Compare: function () {
      var nfc = 'Gr\u00FC\u00DF Gott.';
      var nfd = 'Gru\u0308\u00DF Gott.';

      assertNotEqual(nfc, nfd);
      assertNotEqual(COMPARE_STRING(nfc, nfd), true);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test NORMALIZE_STRING function
// //////////////////////////////////////////////////////////////////////////////

    testICU_Normalize: function () {
      var nfc = 'Gr\u00FC\u00DF Gott.';
      var nfd = 'Gru\u0308\u00DF Gott.';

      assertNotEqual(nfc, nfd);
      assertEqual(NORMALIZE_STRING(nfd), nfc);
      assertEqual(NORMALIZE_STRING(nfd), 'Grüß Gott.');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test skiplist sorting
// //////////////////////////////////////////////////////////////////////////////

    testICU_Compare_Skiplist_Sorting: function () {
      db._drop('ICU_SORTED');
      db._create('ICU_SORTED');
      db.ICU_SORTED.ensureIndex({ type: "skiplist", fields: ["test"] });
      db.ICU_SORTED.save({ test: 'äää' });
      db.ICU_SORTED.save({ test: 'aaa' });
      db.ICU_SORTED.save({ test: 'aab' });
      db.ICU_SORTED.save({ test: 'äaa' });
      db.ICU_SORTED.save({ test: 'äää' });
      db.ICU_SORTED.save({ test: 'Aaa' });

      var y = db.ICU_SORTED.range('test', 'A', 'z');

      assertEqual(y.next().test, 'Aaa');
      assertEqual(y.next().test, 'aaa');
      assertEqual(y.next().test, 'äaa');
      assertEqual(y.next().test, 'äää');
      assertEqual(y.next().test, 'äää');
      assertEqual(y.next().test, 'aab');
      db._drop('ICU_SORTED');
    }

  };
}

jsunity.run(clientTestSuite);

return jsunity.done();
