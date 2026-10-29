# Security and review policy

Changes to the default branch must go through a pull request. The maintainer
uses Codex review as an additional review pass and makes the final merge decision.
Do not merge until the latest revision has been reviewed, findings resolved,
and applicable checks have passed. Missing or unavailable AI review is not approval.
An AI review is not a guarantee that a change is safe.

## Enforced settings versus repository files

CODEOWNERS requests review from @GUMMIBRI; this file alone does not block pushes.
GitHub branch protection/rulesets must separately enforce PRs, code-owner review,
stale approval dismissal, conversation resolution and required checks. Block force
pushes and deletion, apply rules to administrators, and grant no routine bypass.
Verify active settings in GitHub rather than inferring them from this document.

Codex automatic review is configured separately in Codex repository settings:
https://learn.chatgpt.com/docs/third-party/github
Until enabled and verified, request review manually and retain the result on the PR.
A bot comment/reaction alone is not an approving human review or a passing check.
GitHub does not allow a PR author to approve their own PR; owner-authored changes
may need a separate trusted reviewer under an enforced approval rule.

## Review focus

Review all executable paths, downloads, build commands, dependencies/submodule
updates, CI permissions, release scripts and asset parsing. Reject credential
collection, unexplained network access, destructive behavior, hidden payloads,
and unreviewed generated or binary content. Review changes to CODEOWNERS,
AGENTS.md, workflows and this policy with the same care as source changes.

Use GitHub-hosted disposable runners with read-only tokens for untrusted PR code.
Do not run contributor code on a personal machine, self-hosted runner or console
before review. Do not use pull_request_target to execute PR-controlled code with
secrets or write tokens. Do not give PRs production secrets. Keep action versions
pinned, dependency revisions explicit and provenance/third-party notices intact.

## Reporting concerns

Do not post credentials, exploit details or personal data in a public issue.
Use GitHub private vulnerability reporting when available; otherwise contact the
repository owner through an existing private GitHub contact route before sharing
sensitive details. No private reporting service or response-time guarantee is
implied by this document. Suspected malicious changes must stay unmerged.

Retail game images, HD packs, personal saves, credentials and proprietary SDK
material must never enter the public repository or build artifacts.
