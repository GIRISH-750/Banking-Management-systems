# Project Issues & Mistakes Log

## Day 1
**Issue:** Initial entity design included joint-account fields (`is_joint`, `joint_holder_id`) in the Account struct.
**Cause:** These were carried over from an earlier, different course spec (EG-301) that Girish had initially shared, which did require joint accounts. The actual CS-513 spec never mentions joint accounts.
**Fix:** Caught when comparing against the TA's official class diagram — diagram had no joint-account concept, prompting a re-check against the actual PDF, confirming the fields didn't belong. Removed both fields.
**Lesson:** Always re-verify design decisions against the current spec document, not memory of an earlier related spec — especially after a scope/course change mid-project.
