// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::HasSubstr;
using testing::ContainsRegex;

namespace {
const char kTestHtml[] = "/viewsource/test.html";
const char kTestMedia[] = "/media/pink_noise_140ms.wav";
}

class ViewSourceTest : public InProcessBrowserTest {
 public:
  ViewSourceTest() {}

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewSourceTest);
};

// This test renders a page in view-source and then checks to see if the title
// set in the html was set successfully (it shouldn't because we rendered the
// page in view source).
// Flaky; see http://crbug.com/72201.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DoesBrowserRenderInViewSource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our view-source test page.
  GURL url(content::kViewSourceScheme + std::string(":") +
           embedded_test_server()->GetURL(kTestHtml).spec());
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that the title didn't get set.  It should not be there (because we
  // are in view-source mode).
  EXPECT_NE(base::ASCIIToUTF16("foo"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// This test renders a page normally and then renders the same page in
// view-source mode. This is done since we had a problem at one point during
// implementation of the view-source: prefix being consumed (removed from the
// URL) if the URL was not changed (apart from adding the view-source prefix)
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DoesBrowserConsumeViewSourcePrefix) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to google.html.
  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ui_test_utils::NavigateToURL(browser(), url);

  // Then we navigate to the same url but with the "view-source:" prefix.
  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      url.spec());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);

  // The URL should still be prefixed with "view-source:".
  EXPECT_EQ(url_viewsource.spec(),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());
}

// Make sure that when looking at the actual page, we can select "View Source"
// from the menu.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceInMenuEnabledOnANormalPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(chrome::CanViewSource(browser()));
}

// For page that is media content, make sure that we cannot select "View Source"
// See http://crbug.com/83714
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceInMenuDisabledOnAMediaPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kTestMedia));
  ui_test_utils::NavigateToURL(browser(), url);

  const char* mime_type = browser()->tab_strip_model()->GetActiveWebContents()->
      GetContentsMimeType().c_str();

  EXPECT_STREQ("audio/wav", mime_type);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Make sure that when looking at the page source, we can't select "View Source"
// from the menu.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceInMenuDisabledWhileViewingSource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);

  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Tests that reload initiated by the script on the view-source page leaves
// the page in view-source mode.
// Times out on Mac, Windows, ChromeOS Linux: crbug.com/162080
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DISABLED_TestViewSourceReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);
  observer.Wait();

  ASSERT_TRUE(
      content::ExecuteScript(browser()->tab_strip_model()->GetWebContentsAt(0),
                             "window.location.reload();"));

  content::WindowedNotificationObserver observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  observer2.Wait();
  ASSERT_TRUE(browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetController().GetActiveEntry()->IsViewSourceMode());
}

// This test ensures that view-source session history navigations work
// correctly when switching processes. See https://crbug.com/544868.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceCrossProcessAndBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Open another tab to the same origin, so the process is kept alive while
  // the original tab is navigated cross-process. This is required for the
  // original bug to reproduce.
  {
    GURL url = embedded_test_server()->GetURL("/title1.html");
    ui_test_utils::UrlLoadObserver load_complete(
        url, content::NotificationService::AllSources());
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.open('" + url.spec() + "');"));
    load_complete.Wait();
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
  }

  // Switch back to the first tab and navigate it cross-process.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
  EXPECT_TRUE(chrome::CanViewSource(browser()));

  // Navigate back in session history to ensure view-source mode is still
  // active.
  {
    ui_test_utils::UrlLoadObserver load_complete(
        url_viewsource, content::NotificationService::AllSources());
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    load_complete.Wait();
  }

  // Check whether the page is in view-source mode or not by checking if an
  // expected element on the page exists or not. In view-source mode it
  // should not be found.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(document.getElementById('bar') === null);",
      &result));
  EXPECT_TRUE(result);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Tests that view-source mode of b.com subframe won't commit in an a.com (main
// frame) process.  This is a regresion test for https://crbug.com/770946.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, CrossSiteSubframe) {
  // Navigate to a page with a cross-site frame.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Grab the original frames.
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* original_main_frame =
      original_contents->GetMainFrame();
  ASSERT_LE(2u, original_contents->GetAllFrames().size());
  content::RenderFrameHost* original_child_frame =
      original_contents->GetAllFrames()[1];

  // Do a sanity check that in this particular test page the main frame and the
  // subframe are cross-site.
  EXPECT_FALSE(content::SiteInstance::IsSameWebSite(
      browser()->profile(), original_main_frame->GetLastCommittedURL(),
      original_child_frame->GetLastCommittedURL()));
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(original_main_frame->GetProcess()->GetID(),
              original_child_frame->GetProcess()->GetID());
  }

  // Open view-source mode tab for the subframe.  This tries to mimic the
  // behavior of RenderViewContextMenu::ExecuteCommand when it handles
  // IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE.
  content::WebContentsAddedObserver view_source_contents_observer;
  original_child_frame->ViewSource();

  // Grab the view-source frame and wait for load stop.
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  content::RenderFrameHost* view_source_frame =
      view_source_contents->GetMainFrame();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));

  // Verify that the last committed URL is the same in the original and the
  // view-source frames.
  EXPECT_EQ(original_child_frame->GetLastCommittedURL(),
            view_source_frame->GetLastCommittedURL());

  // Verify that the original main frame and the view-source subframe are in a
  // different process (e.g. if the main frame was malicious and the subframe
  // was an isolated origin, then the malicious frame shouldn't be able to see
  // the contents of the isolated document).  See https://crbug.com/770946.
  EXPECT_NE(original_main_frame->GetSiteInstance(),
            view_source_frame->GetSiteInstance());

  // Verify that the original subframe and the view-source subframe are in a
  // different process - see https://crbug.com/699493.
  EXPECT_NE(original_child_frame->GetSiteInstance(),
            view_source_frame->GetSiteInstance());

  // Verify the contents of the view-source tab (should match title1.html).
  std::string source_text;
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      domAutomationController.send(output); )";
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      view_source_contents, view_source_extraction_script, &source_text));
  EXPECT_EQ("<html><head></head><body>This page has no title.</body></html>",
            source_text);

  // Verify the title is derived from the subframe URL.
  GURL original_url = original_child_frame->GetLastCommittedURL();
  std::string title = base::UTF16ToUTF8(view_source_contents->GetTitle());
  EXPECT_THAT(title, HasSubstr(content::kViewSourceScheme));
  EXPECT_THAT(title, HasSubstr(original_url.host()));
  EXPECT_THAT(title, HasSubstr(original_url.port()));
  EXPECT_THAT(title, HasSubstr(original_url.path()));
}

// Tests that "View Source" works fine for pages shown via HTTP POST.
// This is a regression test for https://crbug.com/523.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, HttpPostInMainframe) {
  // Navigate to a page with a form.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_to_echoall.html"));
  ui_test_utils::NavigateToURL(browser(), form_url);
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* original_main_frame =
      original_contents->GetMainFrame();

  // Submit the form and verify that we arrived at the expected location.
  content::TestNavigationObserver form_post_observer(original_contents, 1);
  EXPECT_TRUE(ExecuteScript(original_main_frame,
                            "document.getElementById('form').submit();"));
  form_post_observer.Wait();
  GURL target_url(embedded_test_server()->GetURL("a.com", "/echoall"));
  EXPECT_EQ(target_url, original_main_frame->GetLastCommittedURL());

  // Extract the response nonce.
  std::string response_nonce;
  std::string response_nonce_extraction_script = R"(
      domAutomationController.send(
          document.getElementById('response-nonce').innerText); )";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      original_main_frame, response_nonce_extraction_script, &response_nonce));

  // Open view-source mode tab for the main frame.  This tries to mimic the
  // behavior of RenderViewContextMenu::ExecuteCommand when it handles
  // IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE.
  content::WebContentsAddedObserver view_source_contents_observer;
  original_main_frame->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));

  // Verify contents of the view-source tab.  In particular:
  // 1) the sources should contain the POST data
  // 2) the sources should contain the original response-nonce
  //    (i.e. no new network request should be made - the data should be
  //    retrieved from the cache)
  std::string source_text;
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      domAutomationController.send(output); )";
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      view_source_contents, view_source_extraction_script, &source_text));
  EXPECT_THAT(source_text,
              HasSubstr("<h1>Request Body:</h1><pre>text=value</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<h1>Request Headers:</h1><pre>POST /echoall HTTP"));
  EXPECT_THAT(source_text,
              ContainsRegex("Request Headers:.*Referer: " + form_url.spec()));
  EXPECT_THAT(
      source_text,
      ContainsRegex(
          "Request Headers:.*Content-Type: application/x-www-form-urlencoded"));
  EXPECT_THAT(source_text, HasSubstr("<h1>Response nonce:</h1>"
                                     "<pre id='response-nonce'>" +
                                     response_nonce + "</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<title>EmbeddedTestServer - EchoAll</title>"));

  // Verify that the original contents and the view-source contents are in a
  // different process - see https://crbug.com/699493.
  EXPECT_NE(original_main_frame->GetSiteInstance(),
            view_source_contents->GetMainFrame()->GetSiteInstance());

  // Verify the title of view-source is derived from the URL (not from the title
  // of the original contents).
  std::string title = base::UTF16ToUTF8(view_source_contents->GetTitle());
  EXPECT_EQ("EmbeddedTestServer - EchoAll",
            base::UTF16ToUTF8(original_contents->GetTitle()));
  EXPECT_THAT(title, Not(HasSubstr("EmbeddedTestServer - EchoAll")));
  GURL original_url = original_main_frame->GetLastCommittedURL();
  EXPECT_THAT(title, HasSubstr(content::kViewSourceScheme));
  EXPECT_THAT(title, HasSubstr(original_url.host()));
  EXPECT_THAT(title, HasSubstr(original_url.port()));
  EXPECT_THAT(title, HasSubstr(original_url.path()));
}

// Tests that "View Source" works fine for *subframes* shown via HTTP POST.
// This is a regression test for https://crbug.com/774691.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, HttpPostInSubframe) {
  // Navigate to a page with multiple frames.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate a child frame to a document with a form.
  GURL form_url(embedded_test_server()->GetURL(
      "b.com", "/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(
      content::NavigateIframeToURL(original_contents, "frame1", form_url));
  EXPECT_LE(2u, original_contents->GetAllFrames().size());
  content::RenderFrameHost* original_child_frame =
      original_contents->GetAllFrames()[1];

  // Submit the form and verify that we arrived at the expected location.
  content::TestNavigationObserver form_post_observer(original_contents, 1);
  EXPECT_TRUE(ExecuteScript(original_child_frame,
                            "document.getElementById('form').submit();"));
  form_post_observer.Wait();
  GURL target_url(embedded_test_server()->GetURL("b.com", "/echoall"));
  EXPECT_EQ(target_url, original_child_frame->GetLastCommittedURL());

  // Extract the response nonce.
  std::string response_nonce;
  std::string response_nonce_extraction_script = R"(
      domAutomationController.send(
          document.getElementById('response-nonce').innerText); )";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      original_child_frame, response_nonce_extraction_script, &response_nonce));

  // Open view-source mode tab for the subframe.  This tries to mimic the
  // behavior of RenderViewContextMenu::ExecuteCommand when it handles
  // IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE.
  content::WebContentsAddedObserver view_source_contents_observer;
  original_child_frame->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));

  // Verify contents of the view-source tab.  In particular:
  // 1) the sources should contain the POST data
  // 2) the sources should contain the original response-nonce
  //    (i.e. no new network request should be made - the data should be
  //    retrieved from the cache)
  std::string source_text;
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      domAutomationController.send(output); )";
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      view_source_contents, view_source_extraction_script, &source_text));
  EXPECT_THAT(source_text,
              HasSubstr("<h1>Request Body:</h1><pre>text=value</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<h1>Request Headers:</h1><pre>POST /echoall HTTP"));
  EXPECT_THAT(source_text,
              ContainsRegex("Request Headers:.*Referer: " + form_url.spec()));
  EXPECT_THAT(
      source_text,
      ContainsRegex(
          "Request Headers:.*Content-Type: application/x-www-form-urlencoded"));
  EXPECT_THAT(source_text, HasSubstr("<h1>Response nonce:</h1>"
                                     "<pre id='response-nonce'>" +
                                     response_nonce + "</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<title>EmbeddedTestServer - EchoAll</title>"));

  // Verify that view-source opens in a new process - https://crbug.com/699493.
  EXPECT_NE(original_child_frame->GetSiteInstance(),
            view_source_contents->GetMainFrame()->GetSiteInstance());
  EXPECT_NE(original_contents->GetSiteInstance(),
            view_source_contents->GetMainFrame()->GetSiteInstance());

  // Verify the title is derived from the URL.
  GURL original_url = original_child_frame->GetLastCommittedURL();
  std::string title = base::UTF16ToUTF8(view_source_contents->GetTitle());
  EXPECT_THAT(title, HasSubstr(content::kViewSourceScheme));
  EXPECT_THAT(title, HasSubstr(original_url.host()));
  EXPECT_THAT(title, HasSubstr(original_url.port()));
  EXPECT_THAT(title, HasSubstr(original_url.path()));
}
