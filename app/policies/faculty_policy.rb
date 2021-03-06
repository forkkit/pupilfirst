class FacultyPolicy < ApplicationPolicy
  def index?
    scope.exists?
  end

  def show?
    record.about.present?
  end

  def connect?
    # Cannot connect if connect link is blank.
    return false if record.connect_link.blank?

    if current_founder.present?
      connectable_courses_of_user = Course.joins(:founders).where(founders: { id: user.founders.select(:id) }).where(can_connect: true)

      # Coach must be assigned to one of the connectable courses of the user
      return true if record.courses.where(id: connectable_courses_of_user).exists?
    end

    # Coaches and admins can view all connect links.
    return true if current_coach.present? || current_school_admin.present?

    false
  end

  class Scope < Scope
    def resolve
      # Pupilfirst doesn't have coaches.
      return scope.none if current_school.blank?

      current_school.faculty.where(public: true)
    end
  end
end
