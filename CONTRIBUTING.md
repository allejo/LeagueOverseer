# How to Contribute

Contributions take many forms, from submitting issues, writing docs, 
to making code changes. We welcome it all. Don't forget to sign up for a 
[GitHub account](https://github.com/signup/free), if you haven't already.

## Getting Started

You can clone this repository locally from GitHub using the "Clone in Desktop" 
button from the main project site, or run this command from the command line:

`git clone https://github.com/allejo/LeagueOverseer.git LeagueOverseer`

If you want to make contributions to the project, 
[forking the project](https://help.github.com/articles/fork-a-repo) is the easiest 
way to do this. You can then clone down your fork instead:

`git clone git@github.com:MY-USERNAME-HERE/LeagueOverseer.git LeagueOverseer`

### How is the code organized?

The LeagueOverseer has become one of the largest BZFlag plugins and in order to keep things organized, it has been split up into several files.

- All of the `cpp` and `h` files prefixed with `LeagueOverseer` make up specific parts of the plugin's source code, such as the `Init()` or `Events()` functions.

- LeagueOverseer supports gathering data from matches. All of this data is gathered through MatchEvents and all of the code relating to MatchEvents are prefixed with `MatchEvent`.

- All of the other files are classes used through the plug-in.

### What needs to be done?

Looking at our [**issue tracker**](https://github.com/allejo/LeagueOverseer/issues?state=open) is the quickest way to see what needs to get done. Most issues require in-depth knowledge about the BZFlag plugin API and of LeagueOverseer's code but not all of the issues do! By looking for the [`up-for-grabs`](https://github.com/allejo/LeagueOverseer/labels/up-for-grabs) label in the issue tracker, you will be able to see tasks need to be done and anyone can easily accomplish without prior knowledge of the plugin API and of the plugin's code.

Aside from looking at the issue tracker, the [development roadmap](https://github.com/allejo/LeagueOverseer/blob/master/ROADMAP.md) shows what features are planned for future releases.

If you've found something you'd like to contribute to, leave a comment in the issue so everyone is aware.

## Making Changes

When you're ready to make a change, 
[create a branch](https://help.github.com/articles/fork-a-repo#create-branches) 
off the `master` branch. We use `master` as the default branch for the 
repository, and it holds the most recent contributions, so any changes you make
in master might cause conflicts down the track.

If you make focused commits (instead of one monolithic commit) and have descriptive
commit messages, this will help speed up the review process.

Be sure that you thoroughly test your changes and that your new features do not break existing features.

### Coding Style

The LeagueOverseer project uses `.editorconfig` to keep the coding style as uniform as possible. Please be sure your text editor or IDE properly supports the `.editorconfig` file. If it does not, please install the [respective plugin](http://editorconfig.org/#download) for your IDE or text editor.

For more information, take a look at the [EditorConfig website](http://editorconfig.org/).

#### Coding Practices

- Curly braces (`{` `}`) should always be on their own line

```c++
if (true)
{
    // ...
}
```

- If statements, for/while loops, and switch statements, should always have braces even if only contains a single line of code. This allows for adding more code easily in the future should the need arise.

```c++
if (true)
{
    return false;
}
```

- When defining multiple variables in a section, align the equal signs and data types

```c++
std::string aString = "Hello World";
int         number  = 42;
```

- Do not use `using namespace`; always specify the namespace

```c++
std::string aString = "Hello World";
```

### Submitting Changes

You can publish your branch from the official GitHub app or run this command from
the command line:

`git push origin MY-BRANCH-NAME`

Once your changes are ready to be reviewed, publish the branch to GitHub and
[open a pull request](https://help.github.com/articles/using-pull-requests) 
against it.

A few tips with pull requests:

 - prefix the title with `[WIP]` to indicate this is a work-in-progress. It's
   always good to get feedback early, so don't be afraid to open the pull request 
   before it's "done"
 - use [checklists](https://github.com/blog/1375-task-lists-in-gfm-issues-pulls-comments) 
   to indicate the tasks which need to be done, so everyone knows how close you are to done
 - add comments to the pull request about things that are unclear or you would like suggestions on

Don't forget to mention in the pull request description which issue/issues are 
being addressed.

Some things that will increase the chance that your pull request is accepted.

- Follows the [specified coding style](#coding-style) properly
- Update the documentation, the surrounding comments/docs, examples elsewhere, guides, 
  whatever is affected by your contribution

## Acknowledgements

- Thanks to the [Octokit team](https://github.com/octokit/octokit.net/blob/master/CONTRIBUTING.md) for inspiring these contributions guidelines.
