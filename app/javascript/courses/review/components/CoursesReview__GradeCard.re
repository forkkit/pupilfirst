[%bs.raw {|require("./CoursesReview__GradeCard.css")|}];

open CoursesReview__Types;
let str = React.string;

type status =
  | Graded(bool)
  | Grading
  | Ungraded;

type state = {
  grades: array(Grade.t),
  newFeedback: string,
  saving: bool,
  checklist: array(SubmissionChecklistItem.t),
};

let passed = (grades, evaluationCriteria) =>
  grades
  |> Js.Array.filter(g => {
       let passGrade =
         evaluationCriteria
         |> ArrayUtils.unsafeFind(
              ec =>
                EvaluationCriterion.id(ec)
                == (g |> Grade.evaluationCriterionId),
              "CoursesReview__GradeCard: Unable to find evaluation criterion with id - "
              ++ (g |> Grade.evaluationCriterionId),
            )
         |> EvaluationCriterion.passGrade;

       g |> Grade.value < passGrade;
     })
  |> ArrayUtils.isEmpty;

module CreateGradingMutation = [%graphql
  {|
    mutation($submissionId: ID!, $feedback: String, $grades: [GradeInput!]!, $checklist: JSON!) {
      createGrading(submissionId: $submissionId, feedback: $feedback, grades: $grades, checklist: $checklist){
        success
      }
    }
  |}
];

module UndoGradingMutation = [%graphql
  {|
    mutation($submissionId: ID!) {
      undoGrading(submissionId: $submissionId){
        success
      }
    }
  |}
];

let undoGrading = (submissionId, setState) => {
  setState(state => {...state, saving: true});

  UndoGradingMutation.make(~submissionId, ())
  |> GraphqlQuery.sendQuery
  |> Js.Promise.then_(response => {
       response##undoGrading##success
         ? DomUtils.reload() |> ignore
         : setState(state => {...state, saving: false});
       Js.Promise.resolve();
     })
  |> ignore;
};

let gradeSubmissionQuery =
    (submissionId, state, setState, evaluationCriteria, updateSubmissionCB) => {
  let jsGradesArray = state.grades |> Array.map(g => g |> Grade.asJsType);

  let checklist = state.checklist |> SubmissionChecklistItem.encodeArray;

  setState(state => {...state, saving: true});

  (
    state.newFeedback == ""
      ? CreateGradingMutation.make(
          ~submissionId,
          ~grades=jsGradesArray,
          ~checklist,
          (),
        )
      : CreateGradingMutation.make(
          ~submissionId,
          ~feedback=state.newFeedback,
          ~grades=jsGradesArray,
          ~checklist,
          (),
        )
  )
  |> GraphqlQuery.sendQuery
  |> Js.Promise.then_(response => {
       response##createGrading##success
         ? updateSubmissionCB(
             ~grades=state.grades,
             ~passed=Some(passed(state.grades, evaluationCriteria)),
             ~newFeedback=Some(state.newFeedback),
             ~checklist=state.checklist,
           )
         : ();
       setState(state => {...state, saving: false});
       Js.Promise.resolve();
     })
  |> ignore;
};

let updateGrading = (grade, state, setState) => {
  let newGrades =
    state.grades
    |> Js.Array.filter(g =>
         g
         |> Grade.evaluationCriterionId
         != (grade |> Grade.evaluationCriterionId)
       )
    |> Array.append([|grade|]);

  setState(state => {...state, grades: newGrades});
};
let handleGradePillClick =
    (evaluationCriterionId, value, state, setState, event) => {
  event |> ReactEvent.Mouse.preventDefault;
  switch (setState) {
  | Some(setState) =>
    updateGrading(
      Grade.make(~evaluationCriterionId, ~value),
      state,
      setState,
    )
  | None => ()
  };
};

let findEvaluationCriterion = (evaluationCriteria, evaluationCriterionId) =>
  switch (
    evaluationCriteria
    |> Js.Array.find(ec =>
         ec |> EvaluationCriterion.id == evaluationCriterionId
       )
  ) {
  | Some(ec) => ec
  | None =>
    Rollbar.error(
      "Unable to find evaluation Criterion with id: "
      ++ evaluationCriterionId
      ++ "in CoursesRevew__GradeCard",
    );
    evaluationCriteria[0];
  };

let gradePillHeader = (evaluationCriteriaName, selectedGrade, gradeLabels) =>
  <div className="flex justify-between">
    <p className="text-xs font-semibold"> {evaluationCriteriaName |> str} </p>
    <p className="text-xs font-semibold">
      {(selectedGrade |> string_of_int)
       ++ "/"
       ++ (
         GradeLabel.maxGrade(gradeLabels |> Array.to_list) |> string_of_int
       )
       |> str}
    </p>
  </div>;

let gradePillClasses = (selectedGrade, currentGrade, passgrade, setState) => {
  let defaultClasses =
    "course-review-grade-card__grade-pill border-gray-400 py-1 px-2 text-sm flex-1 font-semibold "
    ++ (
      switch (setState) {
      | Some(_) =>
        "cursor-pointer hover:shadow-lg focus:outline-none "
        ++ (
          currentGrade >= passgrade
            ? "hover:bg-green-500 hover:text-white "
            : "hover:bg-red-500 hover:text-white "
        )
      | None => ""
      }
    );

  defaultClasses
  ++ (
    currentGrade <= selectedGrade
      ? selectedGrade >= passgrade
          ? "bg-green-500 text-white shadow-lg"
          : "bg-red-500 text-white shadow-lg"
      : "bg-white text-gray-900"
  );
};

let showGradePill =
    (key, evaluationCriterion, gradeValue, passGrade, state, setState) =>
  <div
    ariaLabel={
      "evaluation-criterion-"
      ++ (evaluationCriterion |> EvaluationCriterion.id)
    }
    key={key |> string_of_int}
    className="md:pr-8 mt-4">
    {gradePillHeader(
       evaluationCriterion |> EvaluationCriterion.name,
       gradeValue,
       evaluationCriterion |> EvaluationCriterion.gradesAndLabels,
     )}
    <div
      className="course-review-grade-card__grade-bar inline-flex w-full text-center mt-1">
      {evaluationCriterion
       |> EvaluationCriterion.gradesAndLabels
       |> Array.map(gradeLabel => {
            let gradeLabelGrade = gradeLabel |> GradeLabel.grade;

            <div
              key={gradeLabelGrade |> string_of_int}
              onClick={handleGradePillClick(
                evaluationCriterion |> EvaluationCriterion.id,
                gradeLabelGrade,
                state,
                setState,
              )}
              title={gradeLabel |> GradeLabel.label}
              className={gradePillClasses(
                gradeValue,
                gradeLabelGrade,
                passGrade,
                setState,
              )}>
              {switch (setState) {
               | Some(_) => gradeLabelGrade |> string_of_int |> str
               | None => React.null
               }}
            </div>;
          })
       |> React.array}
    </div>
  </div>;

let showGrades = (grades, evaluationCriteria, state) =>
  <div>
    {grades
     |> Grade.sort(evaluationCriteria)
     |> Array.mapi((key, grade) => {
          let gradeEcId = Grade.evaluationCriterionId(grade);
          let ec =
            evaluationCriteria
            |> ArrayUtils.unsafeFind(
                 ec => ec |> EvaluationCriterion.id == gradeEcId,
                 "Unable to find evaluation Criterion with id: "
                 ++ gradeEcId
                 ++ "in CoursesRevew__GradeCard",
               );

          showGradePill(
            key,
            ec,
            grade |> Grade.value,
            ec |> EvaluationCriterion.passGrade,
            state,
            None,
          );
        })
     |> React.array}
  </div>;
let renderGradePills =
    (evaluationCriteria, targetEvaluationCriteriaIds, grades, state, setState) =>
  targetEvaluationCriteriaIds
  |> Array.mapi((key, evaluationCriterionId) => {
       let ec =
         evaluationCriteria
         |> ArrayUtils.unsafeFind(
              e => e |> EvaluationCriterion.id == evaluationCriterionId,
              "CoursesReview__GradeCard: Unable to find evaluation criterion with id - "
              ++ evaluationCriterionId,
            );
       let grade =
         grades
         |> Js.Array.find(g =>
              g
              |> Grade.evaluationCriterionId == (ec |> EvaluationCriterion.id)
            );
       let gradeValue =
         switch (grade) {
         | Some(g) => g |> Grade.value
         | None => 0
         };

       let passGrade = ec |> EvaluationCriterion.passGrade;

       showGradePill(key, ec, gradeValue, passGrade, state, Some(setState));
     })
  |> React.array;
let gradeStatusClasses = (color, status) =>
  "w-12 h-10 p-1 mr-2 md:mr-0 md:w-24 md:h-20 rounded md:rounded-lg border flex justify-center items-center bg-"
  ++ color
  ++ "-100 "
  ++ "border-"
  ++ color
  ++ "-400 "
  ++ (
    switch (status) {
    | Grading => "course-review-grade-card__status-pulse"
    | Graded(_)
    | Ungraded => ""
    }
  );

let submissionStatusIcon = (status, submission, setState) => {
  let (text, color) =
    switch (status) {
    | Graded(passed) => passed ? ("Passed", "green") : ("Failed", "red")
    | Grading => ("Reviewing", "orange")
    | Ungraded => ("Not Reviewed", "gray")
    };

  <div
    ariaLabel="submission-status"
    className="flex w-full md:w-3/6 flex-col items-center justify-center md:border-l mt-4 md:mt-0">
    <div
      className="flex flex-col-reverse md:flex-row items-start md:items-stretch justify-center w-full md:pl-6">
      {switch (submission |> Submission.evaluatedAt, status) {
       | (Some(date), Graded(_)) =>
         <div
           className="bg-gray-200 block md:flex flex-col w-full justify-between rounded-lg pt-3 mr-2 mt-4 md:mt-0">
           <div>
             <p className="text-xs px-3"> {"Evaluated By" |> str} </p>
             <p className="text-sm font-semibold px-3 pb-3">
               {switch (submission |> Submission.evaluatorName) {
                | Some(name) => name |> str
                | None => <em> {"Deleted Coach" |> str} </em>
                }}
             </p>
           </div>
           <div
             className="text-xs bg-gray-300 flex items-center rounded-b-lg px-3 py-2 md:px-3 md:py-1">
             {"on " ++ (date |> Submission.prettyDate) |> str}
           </div>
         </div>
       | (None, Graded(_))
       | (_, Grading)
       | (_, Ungraded) => React.null
       }}
      <div
        className="w-full md:w-24 flex flex-row md:flex-col md:items-center justify-center">
        <div className={gradeStatusClasses(color, status)}>
          {switch (status) {
           | Graded(passed) =>
             passed
               ? <Icon
                   className="if i-badge-check-solid text-xl md:text-5xl text-green-500"
                 />
               : <FaIcon
                   classes="fas fa-exclamation-triangle text-xl md:text-4xl text-red-500"
                 />
           | Grading =>
             <Icon
               className="if i-writing-pad-solid text-xl md:text-5xl text-orange-300"
             />
           | Ungraded =>
             <Icon
               className="if i-eye-solid text-xl md:text-4xl text-gray-400"
             />
           }}
        </div>
        <p
          className={
            "text-xs flex items-center justify-center md:block text-center w-full border rounded px-1 py-px font-semibold md:mt-1 "
            ++ "border-"
            ++ color
            ++ "-400 "
            ++ "bg-"
            ++ color
            ++ "-100 "
            ++ "text-"
            ++ color
            ++ "-800 "
          }>
          {text |> str}
        </p>
      </div>
    </div>
    {switch (submission |> Submission.evaluatedAt, status) {
     | (Some(_), Graded(_)) =>
       <div className="mt-4 md:pl-6 w-full">
         <button
           onClick={_ => undoGrading(submission |> Submission.id, setState)}
           className="btn btn-danger btn-small">
           <i className="fas fa-undo" />
           <span className="ml-2"> {"Undo Grading" |> str} </span>
         </button>
       </div>
     | (None, Graded(_))
     | (_, Grading)
     | (_, Ungraded) => React.null
     }}
  </div>;
};

let updateFeedbackCB = (setState, newFeedback) => {
  setState(state => {...state, newFeedback});
};

let gradeSubmission =
    (
      submissionId,
      state,
      setState,
      evaluationCriteria,
      updateSubmissionCB,
      status,
      event,
    ) => {
  event |> ReactEvent.Mouse.preventDefault;
  switch (status) {
  | Graded(_) =>
    gradeSubmissionQuery(
      submissionId,
      state,
      setState,
      evaluationCriteria,
      updateSubmissionCB,
    )
  | Grading
  | Ungraded => ()
  };
};

let showFeedbackForm =
    (
      grades,
      reviewChecklist,
      updateReviewChecklistCB,
      state,
      setState,
      targetId,
    ) =>
  switch (grades) {
  | [||] =>
    <CoursesReview__FeedbackEditor
      feedback={state.newFeedback}
      label="Add Your Feedback"
      updateFeedbackCB={updateFeedbackCB(setState)}
      reviewChecklist
      updateReviewChecklistCB
      checklistVisible=true
      targetId
    />
  | _ => React.null
  };
let reviewButtonDisabled = status =>
  switch (status) {
  | Graded(_) => false
  | Grading
  | Ungraded => true
  };

let computeStatus = (submission, selectedGrades, evaluationCriteria) =>
  switch (
    submission |> Submission.passedAt,
    submission |> Submission.grades |> ArrayUtils.isNotEmpty,
  ) {
  | (Some(_), _) => Graded(true)
  | (None, true) => Graded(false)
  | (_, _) =>
    if (selectedGrades == [||]) {
      Ungraded;
    } else if (selectedGrades
               |> Array.length != (evaluationCriteria |> Array.length)) {
      Grading;
    } else {
      Graded(passed(selectedGrades, evaluationCriteria));
    }
  };

let submitButtonText = (feedback, grades) =>
  switch (feedback != "", grades |> ArrayUtils.isNotEmpty) {
  | (false, false)
  | (false, true) => "Save grades"
  | (true, false)
  | (true, true) => "Save grades & send feedback"
  };

[@react.component]
let make =
    (
      ~submission,
      ~evaluationCriteria,
      ~reviewChecklist,
      ~updateSubmissionCB,
      ~updateReviewChecklistCB,
      ~targetId,
      ~targetEvaluationCriteriaIds,
    ) => {
  let (state, setState) =
    React.useState(() =>
      {
        grades: [||],
        newFeedback: "",
        saving: false,
        checklist: submission |> Submission.checklist,
      }
    );
  let status = computeStatus(submission, state.grades, evaluationCriteria);
  let updateChecklistCB =
    switch (submission |> Submission.grades) {
    | [||] => Some(checklist => setState(state => {...state, checklist}))
    | _ => None
    };
  let pending = submission |> Submission.grades |> ArrayUtils.isEmpty;

  <DisablingCover disabled={state.saving}>
    <div className=" ">
      <div className="pt-2 pb-6 px-4 md:px-6 bg-gray-100 border-b">
        <SubmissionChecklistShow
          checklist={state.checklist}
          updateChecklistCB
          pending
        />
      </div>
      {showFeedbackForm(
         submission |> Submission.grades,
         reviewChecklist,
         updateReviewChecklistCB,
         state,
         setState,
         targetId,
       )}
      <div className="w-full px-4 pt-4 md:px-6 md:pt-6">
        <h5 className="font-semibold text-sm flex items-center">
          <Icon className="if i-tachometer-regular text-gray-800 text-base" />
          <span className="ml-2 md:ml-3 tracking-wide">
            {"Grade Card" |> str}
          </span>
        </h5>
        <div
          className="flex md:flex-row flex-col border md:ml-7 bg-gray-100 p-2 md:p-4 rounded-lg mt-2">
          <div className="w-full md:w-3/6">
            {switch (submission |> Submission.grades) {
             | [||] =>
               renderGradePills(
                 evaluationCriteria,
                 targetEvaluationCriteriaIds,
                 state.grades,
                 state,
                 setState,
               )

             | grades => showGrades(grades, evaluationCriteria, state)
             }}
          </div>
          {submissionStatusIcon(status, submission, setState)}
        </div>
      </div>
    </div>
    {switch (submission |> Submission.grades) {
     | [||] =>
       <div className="bg-white pt-4 mr-4 ml-4 md:mr-6 md:ml-13">
         <button
           disabled={reviewButtonDisabled(status)}
           className="btn btn-success btn-large w-full border border-green-600"
           onClick={gradeSubmission(
             submission |> Submission.id,
             state,
             setState,
             evaluationCriteria,
             updateSubmissionCB,
             status,
           )}>
           {submitButtonText(state.newFeedback, state.grades) |> str}
         </button>
       </div>

     | _ => React.null
     }}
  </DisablingCover>;
};
