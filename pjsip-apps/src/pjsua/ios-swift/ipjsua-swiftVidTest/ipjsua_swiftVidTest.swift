//
//  ipjsua_swiftVidTest.swift
//  ipjsua-swiftVidTest
//

import XCTest

final class ipjsua_swiftVidTest: XCTestCase {

    override func setUpWithError() throws {
        // Put setup code here. This method is called before the invocation of each test method in the class.

        // In UI tests it is usually best to stop immediately when a failure occurs.
        continueAfterFailure = false

        // In UI tests it’s important to set the initial state - such as interface orientation - required for your tests before they run. The setUp method is a good place to do this.
    }

    override func tearDownWithError() throws {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
    }

    func testExample() throws {
        // UI tests must launch the application that they test.
        let app = XCUIApplication()
        app.launch()

        // Access the Destination text field
        let label = app.staticTexts["Destination:"]
        let textField = app.textFields["sip:test@sip.pjsip.org"]

        // Type localhost into the text field
        textField.tap()
        textField.tap(withNumberOfTaps: 3, numberOfTouches: 1)
        let dest = "sip:localhost:5080"
        textField.typeText(dest)

        // Use XCTAssert to verify the text is correct
        XCTAssertEqual(textField.value as? String, dest)

        // Click "Make call" button
        app.buttons["Make call"].tap()

        // Wait for video to appear
        let expectation = XCTestExpectation(description: "Wait for video")
        let expectedText = "Video"
        waitForElement(label, toHaveText: expectedText, timeout: 6)

        // Capture a screenshot of the entire screen
        let fullScreenshot = app.windows.firstMatch.screenshot()
        // Save the screenshot to a file
        saveScreenshot(image: fullScreenshot)
    }

    // Helper method to wait for an element to have a certain text
    func waitForElement(_ element: XCUIElement, toHaveText text: String,
                        timeout: TimeInterval)
    {
        let predicate = NSPredicate(format: "label CONTAINS[c] %@", text)
        let expectation = XCTNSPredicateExpectation(predicate: predicate,
                                                    object: element)

        let result = XCTWaiter().wait(for: [expectation], timeout: timeout)
        XCTAssertEqual(result, .completed, "Element didn't contain expected" +
                                           " \text within \(timeout) seconds")
    }

    func saveScreenshot(image: XCUIScreenshot) {
        let pngData = image.pngRepresentation
        let documentsDirectory = FileManager.default.urls(
                                 for: .documentDirectory,
                                 in: .userDomainMask).first!
        let fileURL = documentsDirectory.appendingPathComponent("screenshot.png")

        do {
            try pngData.write(to: fileURL)
            print("Screenshot saved to:", fileURL)
        } catch {
            XCTFail("Failed to save screenshot: \(error)")
        }
    }
/*
    func testLaunchPerformance() throws {
        if #available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 7.0, *) {
            // This measures how long it takes to launch your application.
            measure(metrics: [XCTApplicationLaunchMetric()]) {
                XCUIApplication().launch()
            }
        }
    }
*/
}
