# GitHub Branch Protection Rules Configuration

To ensure all code passes unit tests before merging, configure the following branch protection rules:

## Configuration Steps

1. Go to your GitHub repository page
2. Click the **Settings** tab
3. Select **Branches** from the left sidebar
4. Click **Add rule** or edit existing rule

## Recommended Branch Protection Settings

### For main branch:
- ✅ **Require status checks to pass before merging**
  - ✅ **Require branches to be up to date before merging** 
  - Add in **Status checks**: `test`
- ✅ **Require pull request reviews before merging**
  - **Required number of reviews**: 1
- ✅ **Dismiss stale reviews when new commits are pushed**
- ✅ **Require review from code owners** (if CODEOWNERS file exists)
- ✅ **Include administrators** (administrators follow same rules)

### For dev branch:
- ✅ **Require status checks to pass before merging**
  - ✅ **Require branches to be up to date before merging**
  - Add in **Status checks**: `test`
- ✅ **Include administrators**

## Notes

- `test` is the job name defined in the workflow file
- These settings will ensure:
  - All pull requests must pass unit tests before merging
  - Branches must be up to date with target branch
  - main branch requires code review
  - Administrators must follow the same rules

After configuration, any pull request to main or dev branches must pass unit tests before merging.