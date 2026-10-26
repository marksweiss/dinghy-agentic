---
name: web-tool-pages
description: Ship an FM-1 app's browser tool - bundle a release for the Install button, serve it locally, publish it on GitHub Pages, optionally under a custom domain - and a landing page. Use when an app is to be installed from the browser.
---
# The browser tool and GitHub Pages

- **Bundle:** `APP=<app> ./build.sh --release X.Y.Z`, then `tools/web_release.sh <app> X.Y.Z` -> `tools/web/firmware/<app>.fwsc`
  + `release.json` (name, version, identity, link letters, patch ids from app.json). The page installs it with Felucca's
  installer (`fm1ota.js`, vendored unchanged; credit it) and backs up patches over link.h.
- **Local:** `tools/web_tool.sh` (Chrome or Edge; served with caching off, so a reload is never stale).
- **Pages:** in the app's own repository, commit `tools/web/firmware/`, add a workflow that uploads the site
  (`actions/upload-pages-artifact` + `deploy-pages`), enable Pages with build type "workflow"
  (`gh api -X POST repos/<owner>/<repo>/pages -f build_type=workflow`). TastyWaves' `.github/workflows/pages.yml` and
  `tools/site_build.sh` (landing page at /, tool at /tool/) are a working example.
- **Custom domain:** DNS A records 185.199.108-111.153, AAAA 2606:50c0:8000-8003::153, CNAME www -> <owner>.github.io, all
  "DNS only" (Cloudflare's proxy blocks the certificate); verify the domain in GitHub (a TXT record); then
  `gh api -X PUT repos/<owner>/<repo>/pages -f cname=<domain>` and, once the certificate is approved, `-F https_enforced=true`.
- Publishing, domains and DNS are the user's yes, step by step.
