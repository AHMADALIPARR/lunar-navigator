# shoot.py — capture the three demo frames from out/index.html.
# Run with the pw-env python:
#   ~/workspace/.tools/pw-env/bin/python demo/shoot.py
from playwright.sync_api import sync_playwright

CHROME = '/home/hatch/.cache/ms-playwright/chromium-1254/chrome-linux64/chrome'
PAGE = 'file:///home/hatch/workspace/lunar-navigator/out/index.html'

with sync_playwright() as pw:
    b = pw.chromium.launch(executable_path=CHROME,
                           args=['--no-sandbox', '--enable-unsafe-swiftshader'])
    pg = b.new_page(viewport={'width': 1920, 'height': 1080})
    pg.goto(PAGE)
    pg.wait_for_timeout(700)
    pg.screenshot(path='demo/shots/01-table.png')
    pg.evaluate("document.querySelectorAll('h2')[1].scrollIntoView({block: 'start'})")
    pg.wait_for_timeout(400)
    pg.screenshot(path='demo/shots/02-sky.png')
    pg.evaluate("document.querySelectorAll('h2')[2].scrollIntoView({block: 'start'})")
    pg.wait_for_timeout(400)
    pg.screenshot(path='demo/shots/03-ledger.png')
    b.close()
print('shots written to demo/shots/')
